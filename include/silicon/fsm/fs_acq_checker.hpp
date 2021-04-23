/**
 * \file fs_acq_checker.hpp
 *
 * \copyright 2020 John Harwell, All rights reserved.
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

#ifndef INCLUDE_SILICON_FSM_FS_ACQ_CHECKER_HPP_
#define INCLUDE_SILICON_FSM_FS_ACQ_CHECKER_HPP_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "rcppsw/er/client.hpp"

#include "silicon/repr/fs_configuration.hpp"
#include "silicon/silicon.hpp"

/*******************************************************************************
 * Namespaces/Decls
 ******************************************************************************/
namespace cosm::ds {
class cell3D;
} /* namespace cosm::ds */

namespace silicon::repr {
class construction_lane;
} /* namespace silicon::repr */

namespace silicon::controller::perception {
class builder_perception_subsystem;
} /* namespace silicon::controller::perception */

namespace cosm::subsystem {
class sensing_subsystemQ3D;
} /* namespace cosm::subsystem */

NS_START(silicon, fsm);

/*******************************************************************************
 * Class Definitions
 ******************************************************************************/
/**
 * \class fs_acq_checker
 * \ingroup fsm
 *
 * \brief As a robot proceeds along the path from the ingress point to the back
 * of the lane, this class checks to see if it has reached a stygmergic
 * configuration yet, and if so, what type.
 */
class fs_acq_checker : public rer::client<fs_acq_checker> {
 public:
  fs_acq_checker(const csubsystem::sensing_subsystemQ3D* sensing,
                 const scperception::builder_perception_subsystem* perception);

  srepr::fs_configuration operator()(const srepr::construction_lane* lane) const;

  /* Not move/copy constructable/assignable by default */
  fs_acq_checker(const fs_acq_checker&) = delete;
  const fs_acq_checker& operator=(const fs_acq_checker&) = delete;
  fs_acq_checker(fs_acq_checker&&) = delete;
  fs_acq_checker& operator=(fs_acq_checker&&) = delete;

 private:
  struct los_lane_cells {
    const cds::cell3D* ingress{ nullptr };
    const cds::cell3D* egress{ nullptr };
  };

  /**
   * How far ahead of the robot's current cell location on the structure to look
   * for stygmergic configurations, in units of cells. This is a critical value
   * for overall builder FSM provable correctness.
   */
  static constexpr const size_t kDETECT_CELL_DIST = 2;

  los_lane_cells los_cells_calc(const srepr::construction_lane* lane) const;

  srepr::fs_configuration
  configuration_calc(const los_lane_cells& los_cells,
                     const srepr::construction_lane* lane) const;

  /* clang-format off */
  const csubsystem::sensing_subsystemQ3D*           mc_sensing;
  const scperception::builder_perception_subsystem* mc_perception;
  /* clang-format on */
};

NS_END(fsm, silicon);

#endif /* INCLUDE_SILICON_FSM_FS_ACQ_CHECKER_HPP_ */
