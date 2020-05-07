/**
 * \file ct_block_place.hpp
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

#ifndef INCLUDE_SILICON_CONTROLLER_OPERATIONS_CT_BLOCK_PLACE_HPP_
#define INCLUDE_SILICON_CONTROLLER_OPERATIONS_CT_BLOCK_PLACE_HPP_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "rcppsw/patterns/visitor/visitor.hpp"
#include "rcppsw/types/type_uuid.hpp"

#include "cosm/ds/operations/cell3D_op.hpp"

#include "silicon/controller/controller_fwd.hpp"
#include "silicon/fsm/fsm_fwd.hpp"

/*******************************************************************************
 * Namespaces
 ******************************************************************************/
namespace cosm::repr {
class base_block3D;
} /* namespace cosm::repr */

NS_START(silicon, controller, operations, detail);

/*******************************************************************************
 * Class Definitions
 ******************************************************************************/
/**
 * \class ct_block_place
 * \ingroup operations detail
 *
 * \brief Fired whenever a robot picks up a free block in the arena (i.e. one
 * that is not part of a cache).
 */
class ct_block_place : public cdops::cell3D_op {
 private:
  struct visit_typelist_impl {
    using controllers = controller::typelist;
    using fsms = rmpl::typelist<fsm::fcrw_bst_fsm>;
    using value = boost::mpl::joint_view<controllers::type, fsms::type>;
  };

 public:
  using visit_typelist = visit_typelist_impl::value;

  ~ct_block_place(void) override = default;

  ct_block_place(const ct_block_place&) = delete;
  ct_block_place& operator=(const ct_block_place&) = delete;

  /* Single target construction */
  void visit(controller::fcrw_bst_controller& controller);
  void visit(fsm::fcrw_bst_fsm& fsm);

 protected:
  ct_block_place(const rtypes::type_uuid& robot_id,
                        crepr::base_block3D* block);

 private:
  /* clang-format on */
  const rtypes::type_uuid mc_robot_id;
  /* clang-format off */
};

NS_END(detail);

using ct_block_place_visitor = rpvisitor::generic_precise_visitor<detail::ct_block_place>;

NS_END(operations, controller, silicon);

#endif /* INCLUDE_SILICON_CONTROLLER_OPERATIONS_CT_BLOCK_PLACE_HPP_ */
