/**
 * \file structure_ingress_fsm.cpp
 *
 * \copyright 2021 John Harwell, All rights reserved.
 *
 * This file is part of SILICON.
 *
 * SILICON is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * SILICON is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * SILICON.  If not, see <http://www.gnu.org/licenses/
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "silicon/fsm/structure_ingress_fsm.hpp"

#include "cosm/subsystem/saa_subsystemQ3D.hpp"

#include "silicon/controller/perception/builder_perception_subsystem.hpp"
#include "silicon/fsm/calculators/ct_approach.hpp"
#include "silicon/fsm/calculators/ingress_lane_path.hpp"
#include "silicon/fsm/construction_signal.hpp"
#include "silicon/repr/colors.hpp"
#include "silicon/repr/construction_lane.hpp"

/*******************************************************************************
 * Namespaces
 ******************************************************************************/
NS_START(silicon, fsm);

/*******************************************************************************
 * Structure_Ingresss/Destructors
 ******************************************************************************/
structure_ingress_fsm::structure_ingress_fsm(
    const scperception::builder_perception_subsystem* perception,
    csubsystem::saa_subsystemQ3D* const saa,
    rmath::rng* rng)
    : builder_util_fsm(perception, saa, rng, ekST_MAX_STATES),
      ER_CLIENT_INIT("silicon.fsm.structure_ingress"),
      RCPPSW_HFSM_CONSTRUCT_STATE(wait_for_robot, hfsm::top_state()),
      RCPPSW_HFSM_CONSTRUCT_STATE(start, hfsm::top_state()),
      RCPPSW_HFSM_CONSTRUCT_STATE(ct_approach, hfsm::top_state()),
      RCPPSW_HFSM_CONSTRUCT_STATE(ct_entry, hfsm::top_state()),
      RCPPSW_HFSM_CONSTRUCT_STATE(finished, hfsm::top_state()),
      RCPPSW_HFSM_DEFINE_STATE_MAP(
          mc_state_map,
          RCPPSW_HFSM_STATE_MAP_ENTRY_EX(&start),
          RCPPSW_HFSM_STATE_MAP_ENTRY_EX_ALL(&ct_approach,
                                             nullptr,
                                             &entry_ct_approach,
                                             nullptr),
          RCPPSW_HFSM_STATE_MAP_ENTRY_EX_ALL(&wait_for_robot,
                                             nullptr,
                                             &entry_wait_for_robot,
                                             &exit_wait_for_robot),
          RCPPSW_HFSM_STATE_MAP_ENTRY_EX_ALL(&ct_entry,
                                             nullptr,
                                             nullptr,
                                             &exit_ct_entry),
          RCPPSW_HFSM_STATE_MAP_ENTRY_EX(&finished)) {}

structure_ingress_fsm::~structure_ingress_fsm(void) = default;

/*******************************************************************************
 * States
 ******************************************************************************/
RCPPSW_HFSM_STATE_DEFINE_ND(structure_ingress_fsm, start) {
  if (fsm_state::ekST_START != last_state()) {
    ER_DEBUG("Executing ekST_START");
  }
  internal_event(ekST_CT_APPROACH);
  return fsm::construction_signal::ekHANDLED;
}

RCPPSW_HFSM_STATE_DEFINE_ND(structure_ingress_fsm, ct_approach) {
  auto calculator = calculators::ct_approach(
      sensing(), calculators::lane_alignment::kTRAJECTORY_ORTHOGONAL_TOL);
  auto approach = calculator(allocated_lane());

  if (fsm_state::ekST_CT_APPROACH != last_state()) {
    ER_DEBUG("Beginning construction target approach");

    /*
     * If the angle is >= 180 degrees, we are BELOW the X-axis for the
     * structure, and counter-clockwise to the ingress site will be faster. Vice
     * versa for < 180 degrees. See SILICON#41.
     */
    m_ct_approach_polar_sign =
        (approach.ingress_angle >= rmath::radians::kPI) ? -1 : 1;
  }

  if (approach.x_ok && approach.y_ok) {
    ER_INFO("Finished construction target approach");
    auto path = calculators::ingress_lane_path(sensing())(allocated_lane());
    ER_INFO("Calculated target entry path to lane%zu ingress with %zu waypoints",
            allocated_lane()->id(),
            path.size());
    m_ingress_state = std::make_unique<csteer2D::ds::path_state>(path);
    internal_event(ekST_CT_ENTRY);
    return fsm::construction_signal::ekHANDLED;
  }

  if (auto obs = saa()->sensing()->proximity()->avg_prox_obj()) {
    saa()->steer_force2D().accum(saa()->steer_force2D().avoidance(*obs));
  }

  auto light_force =
      saa()->steer_force2D().phototaxis(sensing()->light()->readings());

  auto ingress_pt = allocated_lane()->geometry().ingress_pt();

  /*
   * We need the sign of the orthogonal distance to the ingress lane so that we
   * calculate polar force of the appropriate sign (i.e., not going all the way
   * around in a circle to reach the ingress clockwise when a short
   * counter-clockwise path is MUCH shorter).
   */
  auto polar_force =
      saa()->steer_force2D().polar(ingress_pt.to_2D()) * m_ct_approach_polar_sign;
  const auto* ct = perception()->nearest_ct();
  ER_ASSERT(nullptr != ct,
            "Cannot compute approach forces without construction target");
  /*
   * If the robot is on one of the wrong sides of the structure, we need it to
   * go around the structure in a circular path via polar force, but making
   * sure to stay out of the nest as much as possible, so we weight the polar
   * force pushing the robot away from the nest with the light force
   * attracting it to the nest in proportion to how close we get to the target
   * center.
   */
  double dist_to_center = (sensing()->rpos3D() - ingress_pt).length();
  double light_factor = std::max(
      0.0, (dist_to_center - ct_bc_radius() * 2.0) / kCT_TRANSPORT_BC_DIST_MIN);
  saa()->steer_force2D().accum(light_force * light_factor +
                               polar_force * (1.0 - light_factor));
  saa()->steer_force2D().accum(saa()->steer_force2D().wander(rng()));
  return fsm::construction_signal::ekHANDLED;
}

RCPPSW_HFSM_STATE_DEFINE_ND(structure_ingress_fsm, ct_entry) {
  if (fsm_state::ekST_CT_ENTRY != last_state()) {
    ER_DEBUG("Beginning construction target entry");
  }

  if (auto obs = saa()->sensing()->proximity()->avg_prox_obj()) {
    auto avoidance_force = saa()->steer_force2D().avoidance(*obs);
    saa()->steer_force2D().accum(avoidance_force);
  }

  double path_factor = 1.0;
  bool home_stretch =
      (m_ingress_state->point_index(m_ingress_state->next_point()) ==
       m_ingress_state->n_points() - 1);
  if (home_stretch) {
    /*
     * We only care if we get too close to another robot once we are on the
     * straightaway leading to the ingress point (last point on the path). If
     * that is the case, we want to throttle our path following force
     * proportionally to how much inside the minimum builder trajectory
     * proximity distance we are to help avoid deadlocks of robots clustered
     * near the ingress side of a construction lane.
     *
     * We can't stop and wait for any robots which appear in our trajectory at
     * this point via the WAIT_FOR_ROBOT state, because that can catch robots
     * coming in from our left or right using the polar force and cause
     * deadlocks. This method effectively causes robots to stop by throttling
     * their path following force to near 0, and is much less prone to
     * deadlocks.
     */
    auto thresh = perception()->builder_prox()->trajectory_prox_dist();
    if (auto obs = saa()->sensing()->blobs()->closest_blob(
            srepr::colors::ct_approach())) {
      /*
       * Only throttle if the detected robot is (1) too close, (2) in front of
       * us, and if we are aligned with the ingress lane. Otherwise, it is not a
       * robot we need to worry about yet.
       */
      if (obs->length() < thresh &&
          perception()->self_lane_aligned(allocated_lane()) &&
          !perception()->is_behind_self(*obs, allocated_lane())) {
        path_factor = obs->length() / thresh;
      }
    }
  }

  if (m_ingress_state->is_complete()) {
    auto ingress_pt = allocated_lane()->geometry().ingress_pt();
    ER_DEBUG("Reached lane%zu ingress@%s",
             allocated_lane()->id(),
             rcppsw::to_string(ingress_pt).c_str());
    internal_event(ekST_FINISHED);
  } else {
    auto path_force =
        saa()->steer_force2D().path_following(m_ingress_state.get());
    saa()->steer_force2D().accum(path_force * (path_factor));
  }
  return fsm::construction_signal::ekHANDLED;
}

RCPPSW_HFSM_STATE_DEFINE_ND(structure_ingress_fsm, finished) {
  return rpfsm::event_signal::ekHANDLED;
}

RCPPSW_HFSM_ENTRY_DEFINE_ND(structure_ingress_fsm, entry_ct_approach) {
  /* Turn on light sensor to move towards the nest */
  sensing()->light()->enable();

  /* Turn on LEDs so we can be identified by other robots as we approach */
  actuation()->leds()->set_color(-1, srepr::colors::ct_approach());

  /* Enable camera as we approach so we can see robots in front of us */
  saa()->sensing()->blobs()->enable();
}

RCPPSW_HFSM_EXIT_DEFINE(structure_ingress_fsm, exit_ct_entry) {
  /*
   * Once we are on the target, we don't need the light or proximity sensors
   * anymore.
   */
  sensing()->light()->disable();
  saa()->sensing()->proximity()->disable();
}

RCPPSW_HFSM_ENTRY_DEFINE_ND(structure_ingress_fsm, entry_wait_for_robot) {
  inta_tracker()->inta_enter();
}

RCPPSW_HFSM_EXIT_DEFINE(structure_ingress_fsm, exit_wait_for_robot) {
  inta_tracker()->inta_exit();
}

/*******************************************************************************
 * Taskable Interface
 ******************************************************************************/
void structure_ingress_fsm::task_start(cta::taskable_argument* c_arg) {
  static const std::array<uint8_t, fsm_state::ekST_MAX_STATES> kTRANSITIONS = {
    fsm_state::ekST_CT_APPROACH, /* start */
    rpfsm::event_signal::ekFATAL, /* ct_approach */
    rpfsm::event_signal::ekFATAL, /* wait_for_robot */
    rpfsm::event_signal::ekFATAL, /* ct_entry */
    fsm_state::ekST_CT_APPROACH, /* finished */
  };
  RCPPSW_HFSM_VERIFY_TRANSITION_MAP(kTRANSITIONS, fsm_state::ekST_MAX_STATES);
  auto* const lane = dynamic_cast<repr::construction_lane*>(c_arg);
  ER_ASSERT(nullptr != lane, "Bad construction lane argument");
  allocated_lane(lane);

  external_event(kTRANSITIONS[current_state()]);
} /* task_start() */

void structure_ingress_fsm::task_execute(void) {
  if (event_data_hold()) {
    auto event = event_data_release();
    event->signal(fsm::construction_signal::ekRUN);
    event->type(rpfsm::event_type::ekNORMAL);
    inject_event(std::move(event));
  } else {
    inject_event(fsm::construction_signal::ekRUN, rpfsm::event_type::ekNORMAL);
  }
} /* task_execute() */

/*******************************************************************************
 * Interference Metrics
 ******************************************************************************/

/*******************************************************************************
 * Member Functions
 ******************************************************************************/
void structure_ingress_fsm::init(void) {
  builder_util_fsm::init();
  allocated_lane(nullptr);
  m_ingress_state = nullptr;
} /* init() */

double structure_ingress_fsm::ct_bc_radius(void) const {
  /*
   * Calculate the side of a square inscribed in the "bounding circle"
   * containing the bounding box of the nearest construction target.
   */
  double side = std::max(perception()->nearest_ct()->bbr().x(),
                         perception()->nearest_ct()->bbr().y());

  /* Compute the radius of this circle from the square side length */
  return std::sqrt(std::pow(side, 2) * 2) / 2.0;
} /* ct_bc_radius() */

NS_END(fsm, silicon);
