///
/// Copyright (c) 2014 CNRS
/// Authors: Florent Lamiraux
///
///
// This file is part of hpp-wholebody-step.
// hpp-wholebody-step-planner is free software: you can redistribute it
// and/or modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either version
// 3 of the License, or (at your option) any later version.
//
// hpp-wholebody-step-planner is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Lesser Public License for more details. You should have
// received a copy of the GNU Lesser General Public License along with
// hpp-wholebody-step-planner. If not, see
// <http://www.gnu.org/licenses/>.

#include <boost/assign/list_of.hpp>
#include <hpp/model/humanoid-robot.hh>
#include <hpp/model/joint.hh>
#include <hpp/model/device.hh>
#include <hpp/model/center-of-mass-computation.hh>
#include <hpp/core/config-projector.hh>
#include <hpp/core/numerical-constraint.hh>
#include <hpp/constraints/orientation.hh>
#include <hpp/constraints/position.hh>
#include <hpp/constraints/relative-com.hh>
#include <hpp/constraints/relative-orientation.hh>
#include <hpp/constraints/relative-position.hh>
#include <hpp/constraints/com-between-feet.hh>

#include <hpp/wholebody-step/static-stability-constraint.hh>

namespace hpp {
  namespace wholebodyStep {
    using hpp::constraints::Orientation;
    using hpp::constraints::OrientationPtr_t;
    using hpp::constraints::Position;
    using hpp::constraints::PositionPtr_t;
    using hpp::constraints::RelativeOrientation;
    using hpp::constraints::RelativeComPtr_t;
    using hpp::constraints::RelativeCom;
    using hpp::constraints::RelativeOrientationPtr_t;
    using hpp::constraints::RelativePosition;
    using hpp::constraints::RelativePositionPtr_t;
    using hpp::constraints::ComBetweenFeet;
    using hpp::model::CenterOfMassComputation;
    using hpp::core::NumericalConstraint;
    using hpp::core::ComparisonType;
    using hpp::core::ComparisonTypes;

    std::vector <NumericalConstraintPtr_t> createSlidingStabilityConstraint
    (const DevicePtr_t& robot, const JointPtr_t& leftAnkle,
     const JointPtr_t& rightAnkle, ConfigurationIn_t configuration)
    {
      CenterOfMassComputationPtr_t comc =
        CenterOfMassComputation::create (robot);
      comc->add (robot->rootJoint ());
      comc->computeMass ();
      return createSlidingStabilityConstraint
        (robot, comc, leftAnkle, rightAnkle, configuration);
    }

    std::vector <NumericalConstraintPtr_t> createSlidingStabilityConstraint
    (const DevicePtr_t& robot, const CenterOfMassComputationPtr_t& comc,
     const JointPtr_t& leftAnkle, const JointPtr_t& rightAnkle,
     ConfigurationIn_t configuration)
    {
      std::vector <NumericalConstraintPtr_t> result;
      robot->currentConfiguration (configuration);
      robot->computeForwardKinematics ();
      comc->compute (model::Device::COM);
      JointPtr_t joint1 = leftAnkle;
      JointPtr_t joint2 = rightAnkle;
      const Transform3f& M1 = joint1->currentTransformation ();
      const Transform3f& M2 = joint2->currentTransformation ();
      const vector3_t& x = comc->com ();
      // position of center of mass in left ankle frame
      matrix3_t R1T (M1.getRotation ()); R1T.transpose ();
      vector3_t xloc = R1T * (x - M1.getTranslation ());
      result.push_back (NumericalConstraint::create (RelativeCom::create
            (robot, comc, joint1, xloc)));
      // Relative orientation of the feet
      matrix3_t reference = R1T * M2.getRotation ();
      result.push_back(NumericalConstraint::create (RelativeOrientation::create
		       (robot, joint1, joint2, reference)));
      // Relative position of the feet
      vector3_t local1; local1.setZero ();
      vector3_t global1 = M1.getTranslation ();
      // global1 = R2 local2 + t2
      // local2  = R2^T (global1 - t2)
      matrix3_t R2T (M2.getRotation ()); R2T.transpose ();
      vector3_t local2 = R2T * (global1 - M2.getTranslation ());
      result.push_back (NumericalConstraint::create (RelativePosition::create
			(robot, joint1, joint2, local1, local2)));
      // Orientation of the left foot
      reference.setIdentity ();
      result.push_back (NumericalConstraint::create (Orientation::create
            (robot, joint1, reference, boost::assign::list_of
			      (true)(true)(false))));
      // Position of the left foot
      vector3_t zero; zero.setZero ();
      matrix3_t I3; I3.setIdentity ();
      result.push_back (NumericalConstraint::create (Position::create
			(robot, joint1, zero, M1.getTranslation (), I3,
			 boost::assign::list_of (false)(false)(true))));
      return result;
    }

    std::vector <NumericalConstraintPtr_t> createAlignedCOMStabilityConstraint
    (const DevicePtr_t& robot, const CenterOfMassComputationPtr_t& comc,
     const JointPtr_t& leftAnkle, const JointPtr_t& rightAnkle,
     ConfigurationIn_t configuration)
    {
      using boost::assign::list_of;
      vector3_t zero; zero.setZero ();
      matrix3_t I3; I3.setIdentity ();

      std::vector <NumericalConstraintPtr_t> result;
      NumericalConstraintPtr_t nm;
      robot->currentConfiguration (configuration);
      robot->computeForwardKinematics ();
      comc->compute (model::Device::COM);
      JointPtr_t joint1 = leftAnkle;
      JointPtr_t joint2 = rightAnkle;
      const Transform3f& M1 = joint1->currentTransformation ();
      const Transform3f& M2 = joint2->currentTransformation ();
      // matrix3_t R1T (M1.getRotation ()); R1T.transpose ();
      const vector3_t& x = comc->com ();
      // position of center of mass in left ankle frame
      std::vector <ComparisonType::Type> comps = list_of (ComparisonType::EqualToZero)
       (ComparisonType::EqualToZero) (ComparisonType::Superior)(ComparisonType::Inferior);
      nm = NumericalConstraint::create (
          ComBetweenFeet::create ("ComBetweenFeet", robot, comc,
            joint1, joint2, zero, zero, robot->rootJoint (), x,
            list_of (true)(true)(true)(true)),
          ComparisonTypes::create (comps)
          );
      result.push_back (nm);
      // Orientation of the right foot
      matrix3_t reference;
      reference.setIdentity ();
      nm = NumericalConstraint::create (
          Orientation::create (robot, joint2, reference,
            list_of (true)(true)(false)));
      result.push_back(nm);
      // Orientation of the left foot
      nm = NumericalConstraint::create (
          Orientation::create (robot, joint1, reference,
            list_of (true)(true)(false)));
      result.push_back (nm);
      // Position of the right foot
        nm = NumericalConstraint::create (
          Position::create (robot, joint2, zero, M2.getTranslation (), I3,
			    boost::assign::list_of (false)(false)(true)));
      result.push_back (nm);
      // Position of the left foot
      nm = NumericalConstraint::create (
          Position::create (robot, joint1, zero, M1.getTranslation (), I3,
			    boost::assign::list_of (false)(false)(true)));
      result.push_back (nm);
      return result;
    }
  } // namespace wholebodyStep
} // namespace hpp
