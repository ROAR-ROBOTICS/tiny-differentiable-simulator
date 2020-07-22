#include <fenv.h>
#include <stdio.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "neural_push_utils.h"
#include "pybullet_visualizer_api.h"
#include "tiny_ceres_estimator.h"
#include "tiny_dataset.h"
#include "tiny_double_utils.h"
#include "tiny_file_utils.h"
#include "tiny_multi_body.h"
#include "tiny_system_constructor.h"

typedef PyBulletVisualizerAPI VisualizerAPI;

// whether to use Parallel Basin Hopping
#define USE_PBH true
const int param_dim = 3;
const int state_dim = 3;
const ResidualMode res_mode = RES_MODE_1D;

const int residual_dim =
    TinyCeresEstimator<param_dim, state_dim, res_mode>::kResidualDim;

typedef ceres::Jet<double, param_dim> ADScalar;
typedef CeresUtils<param_dim> ADUtils;

template <typename Scalar, typename Utils>
struct Laboratory {
  // create duplicate world object to not get any automatic collision response
  // between object and tip (we create our own contact point for this
  // interaction)
  TinyWorld<Scalar, Utils> world_ground, world_tip;
  TinyMultiBody<Scalar, Utils> *ground{nullptr};
  TinyMultiBody<Scalar, Utils> *tip{nullptr};

  TinyMultiBody<Scalar, Utils> *true_object{nullptr};
  TinyMultiBody<Scalar, Utils> *object{nullptr};
  TinyDataset<double, 2> object_exterior;

  // object exterior transformed by object pose
  TinyDataset<Scalar, 2> tf_object_exterior;

  TinyMultiBodyConstraintSolverSpring<Scalar, Utils> *contact_ground{nullptr};
  TinyMultiBodyConstraintSolver<Scalar, Utils> tip_contact_model;

  Laboratory(TinyUrdfCache<Scalar, Utils> &urdf_cache,
             const std::string &shape_urdf_filename,
             const std::string &surface_urdf_filename,
             const std::string &tip_urdf_filename,
             const std::string &exterior_filename, VisualizerAPI *sim,
             VisualizerAPI *sim2) {
    bool ignore_cache = true;
    ground = urdf_cache.construct(surface_urdf_filename, world_ground, sim2,
                                  sim, ignore_cache);

    true_object = urdf_cache.construct(shape_urdf_filename, world_tip, sim2,
                                       sim, ignore_cache);
    object = urdf_cache.construct(shape_urdf_filename, world_ground, sim2, sim,
                                  ignore_cache);

    // make groundtruth object green
    for (auto &link : true_object->m_links) {
      for (auto visual_id : link.m_visual_uids1) {
        b3RobotSimulatorChangeVisualShapeArgs vargs;
        vargs.m_objectUniqueId = visual_id;
        vargs.m_hasRgbaColor = true;
        vargs.m_rgbaColor = btVector4(0.1, 0.6, 0, 0.7);
        sim->changeVisualShape(vargs);
      }
    }

    TinyNumpyReader<double, 2> npy_reader;
    bool npy_success = npy_reader.Open(exterior_filename);
    assert(npy_success);
    object_exterior = npy_reader.Read();
    tf_object_exterior.Resize(object_exterior.Shape());

    tip = urdf_cache.construct(tip_urdf_filename, world_tip, sim2, sim);

    {
      // set up contact (friction) model for surface <-> object contact
      delete world_ground.m_mb_constraint_solver;
      contact_ground = new TinyMultiBodyConstraintSolverSpring<Scalar, Utils>;
      world_ground.m_mb_constraint_solver = contact_ground;

      // use some sensible initial settings
      contact_ground->spring_k = Scalar(1);
      contact_ground->damper_d = Scalar(0.1);
      contact_ground->mu_static = Scalar(0.1);
      // contact_ground->friction_model = FRICTION_NONE;
      world_ground.default_friction = Scalar(0.5);
    }

    world_tip.default_friction = Scalar(0.25);
  }

  ~Laboratory() { delete contact_ground; }
};

class PushEstimator
    : public TinyCeresEstimator<param_dim, state_dim, res_mode> {
 public:
  typedef TinyCeresEstimator<param_dim, state_dim, res_mode> CeresEstimator;
  using CeresEstimator::kStateDim, CeresEstimator::kParameterDim;
  using CeresEstimator::parameters, CeresEstimator::dt;
  using typename CeresEstimator::ADScalar;

  std::vector<double> initial_params;

  // stores trajectories per surface-shape combination
  std::vector<PushData> trajectories;

  mutable std::map<std::string, Laboratory<double, DoubleUtils> *> labs_double;
  mutable std::map<std::string, Laboratory<ADScalar, ADUtils> *> labs_ad;

  std::size_t skip_steps = 10;

  PushEstimator() : CeresEstimator(0.0) {
    parameters[0] = {"mu_kinetic", 0.5, 0.1, 1.5};
    parameters[1] = {"mu_static", 0.5, 0.1, 1.5};
    parameters[2] = {"v_transition", 0.01, 0.0001, 0.2};
    // for (int i = 0; i < neural_param_dim; ++i) {
    //   double regularization = 1;
    //   parameters[i + analytical_param_dim] = {"nn_weight_" +
    //   std::to_string(i),
    //                                           double(rand()) / RAND_MAX,
    //                                           -1., 1., regularization};
    // }
    for (const auto &p : parameters) {
      initial_params.push_back(p.value);
    }
  }

  ~PushEstimator() {
    for (auto &entry : labs_ad) {
      delete entry.second;
    }
    for (auto &entry : labs_double) {
      delete entry.second;
    }
  }

  void add_training_dataset(const std::string &h5_filename, VisualizerAPI *sim,
                            VisualizerAPI *sim2) {
    PushData data(h5_filename);
    trajectories.push_back(data);
    target_times.push_back(data.time);
    target_trajectories.push_back(data.states);
    add_laboratory<double, DoubleUtils>(urdf_cache_double, &labs_double, data,
                                        sim, sim2);
    add_laboratory<ADScalar, ADUtils>(urdf_cache_ad, &labs_ad, data, sim, sim2);
  }

  void rollout(const std::vector<ADScalar> &params,
               std::vector<std::vector<ADScalar>> &output_states, double &dt,
               std::size_t ref_id) const override {
    this->template rollout<ADScalar, ADUtils>(params, output_states, dt,
                                              ref_id);
  }
  void rollout(const std::vector<double> &params,
               std::vector<std::vector<double>> &output_states, double &dt,
               std::size_t ref_id) const override {
    this->template rollout<double, DoubleUtils>(params, output_states, dt,
                                                ref_id);
  }

 private:
  template <typename Scalar, typename Utils>
  static void add_laboratory(
      TinyUrdfCache<Scalar, Utils> &urdf_cache,
      std::map<std::string, Laboratory<Scalar, Utils> *> *labs,
      const PushData &data, VisualizerAPI *sim, VisualizerAPI *sim2) {
    if (labs->find(data.lab_name) != labs->end()) {
      return;
    }

    std::string shape_urdf_filename, surface_urdf_filename, tip_urdf_filename,
        exterior_filename;

    TinyFileUtils::find_file("mit-push/obj/" + data.shape_name + ".urdf",
                             shape_urdf_filename);
    TinyFileUtils::find_file("mit-push/obj/" + data.surface_name + ".urdf",
                             surface_urdf_filename);
    TinyFileUtils::find_file("mit-push/obj/tip.urdf", tip_urdf_filename);
    TinyFileUtils::find_file("mit-push/obj/" + data.shape_name + "_ext.npy",
                             exterior_filename);

    (*labs)[data.lab_name] = new Laboratory<Scalar, Utils>(
        urdf_cache, shape_urdf_filename, surface_urdf_filename,
        tip_urdf_filename, exterior_filename, sim, sim2);
  }

  template <typename Scalar>
  constexpr std::conditional_t<std::is_same_v<Scalar, double>,
                               Laboratory<double, DoubleUtils> &,
                               Laboratory<ADScalar, ADUtils> &>
  get_lab(const std::string &lab_name) const {
    if constexpr (std::is_same_v<Scalar, double>) {
      return *(labs_double[lab_name]);
    } else {
      return *(labs_ad[lab_name]);
    }
  }

  TinyUrdfCache<double, DoubleUtils> urdf_cache_double;
  TinyUrdfCache<ADScalar, ADUtils> urdf_cache_ad;

  template <typename Scalar>
  constexpr auto &get_cache() {
    if constexpr (std::is_same_v<Scalar, double>) {
      return urdf_cache_double;
    } else {
      return urdf_cache_ad;
    }
  }

 public:
  template <typename Scalar, typename Utils>
  void rollout(const std::vector<Scalar> &params,
               std::vector<std::vector<Scalar>> &output_states, double &dt,
               std::size_t ref_id, VisualizerAPI *sim = nullptr) const {
    const auto &data = trajectories[ref_id];
    dt = data.dt * skip_steps;
    this->dt = dt;

    Laboratory<Scalar, Utils> &lab =
        this->template get_lab<Scalar>(data.lab_name);
    auto *tip = lab.tip;
    tip->initialize();

    lab.world_ground.default_friction = params[0];
    lab.contact_ground->mu_static = params[1];
    lab.contact_ground->v_transition = params[2];

    auto *object = lab.object;
    object->initialize();
    object->m_q[0] = Scalar(data.object_x[0]);
    object->m_q[1] = Scalar(data.object_y[0]);
    object->m_q[2] = Scalar(0.005);  // initial object height
    object->m_q[3] = Scalar(data.object_yaw[0]);
    object->forward_kinematics();

    auto &world_ground = lab.world_ground;

    double sim_dt = data.dt;
    const Scalar sdt = Scalar(sim_dt);

    for (std::size_t i = 0; i < data.time.size(); ++i) {
      tip->m_q[0] = Scalar(data.tip_x[i]);
      tip->m_q[1] = Scalar(data.tip_y[i]);
      if (i > 0) {
        tip->m_qd[0] = Scalar((data.tip_x[i] - data.tip_x[i - 1]) / sim_dt);
        tip->m_qd[1] = Scalar((data.tip_y[i] - data.tip_y[i - 1]) / sim_dt);
      }
      tip->forward_kinematics();

      object->forward_dynamics(world_ground.get_gravity());
      object->clear_forces();
      transform_points<Scalar, Utils>(lab.object_exterior,
                                      lab.tf_object_exterior, object->m_q[0],
                                      object->m_q[1], object->m_q[3]);
      auto tip_contact =
          compute_contact<Scalar, Utils>(tip, object, lab.tf_object_exterior);
      lab.tip_contact_model.resolveCollision({tip_contact}, sdt);
      world_ground.step(sdt);
      object->integrate(sdt);
      object->forward_kinematics();

      if (i % skip_steps == 0) {
        output_states.push_back(
            {object->m_q[0], object->m_q[1], object->m_q[3]});
      }

      if (sim) {
        TinyMultiBody<Scalar, Utils> *true_object = lab.true_object;
        true_object->m_q[0] = Scalar(data.object_x[i]);
        true_object->m_q[1] = Scalar(data.object_y[i]);
        true_object->m_q[3] = Scalar(data.object_yaw[i]);
        true_object->forward_kinematics();

        PyBulletUrdfImport<Scalar, Utils>::sync_graphics_transforms(tip, *sim);
        PyBulletUrdfImport<Scalar, Utils>::sync_graphics_transforms(object,
                                                                    *sim);
        PyBulletUrdfImport<Scalar, Utils>::sync_graphics_transforms(true_object,
                                                                    *sim);
        std::this_thread::sleep_for(
            std::chrono::duration<double>(Utils::getDouble(sim_dt)));
      }
    }
  }
};

int main(int argc, char *argv[]) {
  typedef double Scalar;
  typedef DoubleUtils Utils;

  std::string connection_mode = "gui";

  std::string shape = "butter";

  std::string object_filename;
  TinyFileUtils::find_file("mit-push/obj/" + shape + ".urdf", object_filename);
  std::string tip_filename;
  TinyFileUtils::find_file("mit-push/obj/tip.urdf", tip_filename);
  std::string ground_filename;
  TinyFileUtils::find_file("mit-push/obj/plywood.urdf", ground_filename);

  std::string exterior_filename;
  TinyFileUtils::find_file("mit-push/obj/" + shape + "_ext.npy",
                           exterior_filename);
  TinyNumpyReader<Scalar, 2> npy_reader;
  bool npy_success = npy_reader.Open(exterior_filename);
  assert(npy_success);
  auto exterior = npy_reader.Read();

  std::string push_filename;
  TinyFileUtils::find_file("mit-push/abs/" + shape + "/" + shape +
                               "_h5/"
                               "motion_surface=abs_shape=" +
                               shape + "_a=0_v=10_i=0.000_s=0.000_t=0.000.h5",
                           push_filename);

  PushData data(push_filename);

  if (argc > 1) object_filename = std::string(argv[1]);
  bool floating_base = true;

  // Set NaN trap
  // feenableexcept(FE_INVALID | FE_OVERFLOW);

  printf("floating_base=%d\n", floating_base);
  printf("object_filename=%s\n", object_filename.c_str());
  printf("tip_filename=%s\n", tip_filename.c_str());
  VisualizerAPI *sim2 = new VisualizerAPI();
  bool isConnected2 = sim2->connect(eCONNECT_DIRECT);

  VisualizerAPI *sim = new VisualizerAPI();

  printf("connection_mode=%s\n", connection_mode.c_str());
  int mode = eCONNECT_GUI;
  if (connection_mode == "direct") mode = eCONNECT_DIRECT;
  if (connection_mode == "gui") mode = eCONNECT_GUI;
  if (connection_mode == "shared_memory") mode = eCONNECT_SHARED_MEMORY;

  bool isConnected = sim->connect(mode);
  if (!isConnected) {
    printf("Cannot connect\n");
    return -1;
  }

  sim->resetSimulation();
  sim->setTimeOut(10);

  std::function<std::unique_ptr<PushEstimator>()> construct_estimator =
      [&push_filename]() {
        auto estimator = std::make_unique<PushEstimator>();
        estimator->options.minimizer_progress_to_stdout = !USE_PBH;
        estimator->options.max_num_consecutive_invalid_steps = 100;
        estimator->options.max_num_iterations = 200;
        // divide each cost term by integer time step ^ 2 to reduce gradient
        // explosion
        estimator->divide_cost_by_time_factor = 0.;
        // estimator->divide_cost_by_time_exponent = 1.2;

        VisualizerAPI *sim_direct = new VisualizerAPI();
        bool isConnected2 = sim_direct->connect(eCONNECT_DIRECT);
        estimator->add_training_dataset(push_filename, sim_direct, sim_direct);
        estimator->use_finite_diff = true;
        return estimator;
      };

  std::vector<double> best_params;
  PushEstimator estimator;
  estimator.add_training_dataset(push_filename, sim, sim2);
  estimator.use_finite_diff = true;
  estimator.setup();

  // best_params = {0.1000022164, 0.9283739043, 0.1999897444};
  // best_params = {0.5, 0.5, 0.01};

#if USE_PBH
  std::array<double, param_dim> initial_guess;
  for (int i = 0; i < param_dim; ++i) {
    initial_guess[i] = 0.0;
  }
  BasinHoppingEstimator<param_dim, PushEstimator> bhe(construct_estimator,
                                                      initial_guess);
  bhe.time_limit = 100;
  bhe.run();

  printf("Optimized parameters:");
  for (int i = 0; i < param_dim; ++i) {
    printf(" %.8f", bhe.params[i]);
  }
  printf("\n");

  printf("Best cost: %f\n", bhe.best_cost());

  for (const auto &p : bhe.params) {
    best_params.push_back(p);
  }
#else
  double cost = 0;
  double gradient[param_dim] = {0,0,0};
  estimator.compute_loss(estimator.vars(), &cost, gradient);

  printf("Cost: %.6f\n", cost);

  printf("Gradient:  ");
  for (int i = 0; i < param_dim; ++i) {
    printf("%.6f  ", gradient[i]);
  }

  auto summary = estimator.solve();
  std::cout << summary.FullReport() << std::endl;
  std::cout << "Final cost: " << summary.final_cost << "\n";

  for (const auto &p : estimator.parameters) {
    best_params.push_back(p.value);
  }

  std::ofstream file("param_evolution.txt");
  for (const auto &params : estimator.parameter_evolution()) {
    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
      file << params[i];
      if (i < static_cast<int>(params.size()) - 1) file << "\t";
    }
    file << "\n";
  }
  file.close();
#endif

  for (int i = 0; i < param_dim; ++i) {
    const auto &p = estimator.parameters[i];
    printf("%s: %.10f\n", p.name.c_str(), best_params[i]);
  }

  printf("\n\n");
  fflush(stdout);

  while (true) {
    std::vector<std::vector<double>> output_states;
    estimator.template rollout<double, DoubleUtils>(best_params, output_states,
                                                     data.dt, 0, sim);
    std::this_thread::sleep_for(std::chrono::duration<double>(5.));
  }

  return 0;

  TinyUrdfCache<Scalar, Utils> urdf_cache;

  // create duplicate world object to not get any automatic collision response
  // between object and tip (we create our own contact point for this
  // interaction)
  TinyWorld<Scalar, Utils> world, world2;
  world.set_gravity(TinyVector3<Scalar, Utils>(0., 0., -9.81));

  TinyMultiBody<Scalar, Utils> *ground =
      urdf_cache.construct(ground_filename, world, sim2, sim);
  bool ignore_cache = true;
  TinyMultiBody<Scalar, Utils> *true_object =
      urdf_cache.construct(object_filename, world2, sim2, sim, ignore_cache);
  TinyMultiBody<Scalar, Utils> *object =
      urdf_cache.construct(object_filename, world, sim2, sim, ignore_cache);
  TinyMultiBody<Scalar, Utils> *tip =
      urdf_cache.construct(tip_filename, world2, sim2, sim);

  {
    // set up contact (friction) model for surface <-> object contact
    delete world.m_mb_constraint_solver;
    auto *spring_contact =
        new TinyMultiBodyConstraintSolverSpring<Scalar, Utils>;
    spring_contact->spring_k = 1;
    spring_contact->damper_d = 0.1;
    spring_contact->mu_static = 0.1;
    // spring_contact->friction_model = FRICTION_NONE;
    world.m_mb_constraint_solver = spring_contact;
    world.default_friction = 0.5;
  }

  // Choose solver for contact between tip and object
  TinyMultiBodyConstraintSolver<Scalar, Utils> tip_contact_model;
  // TinyMultiBodyConstraintSolverSpring<Scalar, Utils> tip_contact_model;
  // friction between tip and object from the paper
  world2.default_friction = 0.25;

  fflush(stdout);

  // double dt = 1. / 1000.;
  double time = 0;

  for (auto &link : true_object->m_links) {
    for (auto visual_id : link.m_visual_uids1) {
      b3RobotSimulatorChangeVisualShapeArgs vargs;
      vargs.m_objectUniqueId = visual_id;
      vargs.m_hasRgbaColor = true;
      vargs.m_rgbaColor = btVector4(0.1, 0.6, 0, 0.7);
      sim->changeVisualShape(vargs);
    }
  }

  std::size_t skip_steps = 1;
  Scalar dt = skip_steps * data.dt;

  printf("dt: %.5f\n", Utils::getDouble(dt));

  while (sim->isConnected()) {
    printf("Playback...\n");

    object->initialize();
    object->m_q[0] = data.object_x[0];
    object->m_q[1] = data.object_y[0];
    object->m_q[2] = 0.02;
    object->m_q[3] = (data.object_yaw[0]);
    object->forward_kinematics();
    PyBulletUrdfImport<Scalar, Utils>::sync_graphics_transforms(object, *sim);

    for (std::size_t i = 0; i < data.time.size(); i += skip_steps) {
      tip->m_q[0] = data.tip_x[i];
      tip->m_q[1] = data.tip_y[i];
      if (i > 0) {
        tip->m_qd[0] = (data.tip_x[i] - data.tip_x[i - 1]) / data.dt;
        tip->m_qd[1] = (data.tip_y[i] - data.tip_y[i - 1]) / data.dt;
      }
      tip->forward_kinematics();
      PyBulletUrdfImport<Scalar, Utils>::sync_graphics_transforms(tip, *sim);

      object->forward_dynamics(world.get_gravity());
      object->clear_forces();
      auto tip_contact = compute_contact(tip, object, exterior);
      tip_contact_model.resolveCollision({tip_contact}, dt);
      world.step(dt);
      object->integrate(dt);
      object->forward_kinematics();
      PyBulletUrdfImport<Scalar, Utils>::sync_graphics_transforms(object, *sim);

      true_object->m_q[0] = data.object_x[i];
      true_object->m_q[1] = data.object_y[i];
      true_object->m_q[3] = (data.object_yaw[i]);
      true_object->forward_kinematics();
      PyBulletUrdfImport<Scalar, Utils>::sync_graphics_transforms(true_object,
                                                                  *sim);

      std::this_thread::sleep_for(std::chrono::duration<double>(dt));
    }
    std::this_thread::sleep_for(std::chrono::duration<double>(5.));
  }

  delete sim;
  delete sim2;

  return EXIT_SUCCESS;
}
