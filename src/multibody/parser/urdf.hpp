//
// Copyright (c) 2015 CNRS
// Copyright (c) 2015 Wandercraft, 86 rue de Paris 91400 Orsay, France.
//
// This file is part of Pinocchio
// Pinocchio is free software: you can redistribute it
// and/or modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either version
// 3 of the License, or (at your option) any later version.
//
// Pinocchio is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Lesser Public License for more details. You should have
// received a copy of the GNU Lesser General Public License along with
// Pinocchio If not, see
// <http://www.gnu.org/licenses/>.

#ifndef __se3_urdf_hpp__
#define __se3_urdf_hpp__

#include <urdf_model/model.h>
#include <urdf_parser/urdf_parser.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <boost/foreach.hpp>
#include "pinocchio/multibody/model.hpp"

#include <exception>

namespace urdf
{
  typedef boost::shared_ptr<ModelInterface> ModelInterfacePtr;
  typedef boost::shared_ptr<const Joint> JointConstPtr;
  typedef boost::shared_ptr<const Link> LinkConstPtr;
  typedef boost::shared_ptr<Link> LinkPtr;
  typedef boost::shared_ptr<const Inertial> InertialConstPtr;
}

namespace se3
{
  namespace urdf
  {

    ///
    /// \brief Convert URDF Inertial quantity to Spatial Inertia.
    ///
    /// \param[in] Y The input URDF Inertia.
    ///
    /// \return The converted Spatial Inertia se3::Inertia.
    ///
    inline Inertia convertFromUrdf( const ::urdf::Inertial& Y )
    {
      const ::urdf::Vector3 & p = Y.origin.position;
      const ::urdf::Rotation & q = Y.origin.rotation;

      const Eigen::Vector3d com(p.x,p.y,p.z);
      const Eigen::Matrix3d & R = Eigen::Quaterniond(q.w,q.x,q.y,q.z).matrix();

      Eigen::Matrix3d I; I << Y.ixx,Y.ixy,Y.ixz
			   ,  Y.ixy,Y.iyy,Y.iyz
			   ,  Y.ixz,Y.iyz,Y.izz;
      return Inertia(Y.mass,com,R*I*R.transpose());
    }

    ///
    /// \brief Convert URDF Pose quantity to SE3.
    ///
    /// \param[in] M The input URDF Pose.
    ///
    /// \return The converted pose/transform se3::SE3.
    ///
    inline SE3 convertFromUrdf( const ::urdf::Pose & M )
    {
      const ::urdf::Vector3 & p = M.position;
      const ::urdf::Rotation & q = M.rotation;
      return SE3( Eigen::Quaterniond(q.w,q.x,q.y,q.z).matrix(), Eigen::Vector3d(p.x,p.y,p.z));
    }

    ///
    /// \brief The four possible cartesian types of an 3D axis.
    ///
    enum AxisCartesian { AXIS_X, AXIS_Y, AXIS_Z, AXIS_UNALIGNED };

   
    ///
    /// \brief Extract the cartesian property of a particular 3D axis.
    ///
    /// \param[in] axis The input URDF axis.
    ///
    /// \return The property of the particular axis se3::urdf::AxisCartesian.
    ///
    inline AxisCartesian extractCartesianAxis( const ::urdf::Vector3 & axis )
    {
      if( (axis.x==1.0)&&(axis.y==0.0)&&(axis.z==0.0) )
	return AXIS_X;
      else if( (axis.x==0.0)&&(axis.y==1.0)&&(axis.z==0.0) )
	return AXIS_Y;
      else if( (axis.x==0.0)&&(axis.y==0.0)&&(axis.z==1.0) )
	return AXIS_Z;
      else
	return AXIS_UNALIGNED;
    }

    ///
    /// \brief Recursive procedure for reading the URDF tree.
    ///        The function returns an exception as soon as a necessary Inertia or Joint information are missing.
    ///
    /// \param[in] link The current URDF link.
    /// \param[in] model The model where the link must be added.
    /// \param[in] placementOffset The relative placement of the link relative to the closer non fixed joint in the tree.
    ///
    inline void parseTree(::urdf::LinkConstPtr link, Model & model, const SE3 & placementOffset = SE3::Identity(), bool verbose = false) throw (std::invalid_argument)
{


  ::urdf::JointConstPtr joint = link->parent_joint;
  SE3 nextPlacementOffset = SE3::Identity(); // OffSet of the next link. In case we encounter a fixed joint, we need to propagate the length of its attached body to next joint.

  // std::cout << " *** " << link->name << "    < attached by joint ";
  // if(joint)
  //   std::cout << "#" << link->parent_joint->name << std::endl;
  // else std::cout << "###ROOT" << std::endl;

  // std::cout << "placementOffset: " << placementOffset << std::endl;

  if(joint!=NULL)
  {
    assert(link->getParent()!=NULL);
    
    const std::string & joint_name = joint->name;
    const std::string & link_name = link->name;
    const std::string & parent_link_name = link->getParent()->name;
    std::string joint_info = "";
    if (!link->inertial && joint->type != ::urdf::Joint::FIXED)
    {
      const std::string exception_message (link->name + " - spatial inertia information missing.");
      throw std::invalid_argument(exception_message);
    }

    Model::Index parent = (link->getParent()->parent_joint==NULL) ? (model.existJointName("root_joint") ? model.getJointId("root_joint") : 0) :
                                                                    model.getJointId( link->getParent()->parent_joint->name );
    //std::cout << joint->name << " === " << parent << std::endl;

    const SE3 & jointPlacement = placementOffset*convertFromUrdf(joint->parent_to_joint_origin_transform);
    
    const Inertia & Y = (link->inertial) ?  convertFromUrdf(*link->inertial) :
    Inertia::Identity();
    
    bool visual = (link->visual) ? true : false;


    //std::cout << "Parent = " << parent << std::endl;
    //std::cout << "Placement = " << (Matrix4)jointPlacement << std::endl;

    switch(joint->type)
    {
      case ::urdf::Joint::REVOLUTE:
      {
        joint_info = "joint REVOLUTE with axis";
        Eigen::VectorXd maxEffort;
        Eigen::VectorXd velocity;
        Eigen::VectorXd lowerPosition;
        Eigen::VectorXd upperPosition;
        
        if (joint->limits)
        {
          maxEffort.resize(1);      maxEffort     << joint->limits->effort;
          velocity.resize(1);       velocity      << joint->limits->velocity;
          lowerPosition.resize(1);  lowerPosition << joint->limits->lower;
          upperPosition.resize(1);  upperPosition << joint->limits->upper;
        }
        
        Eigen::Vector3d jointAxis(Eigen::Vector3d::Zero());
        AxisCartesian axis = extractCartesianAxis(joint->axis);
          
        switch(axis)
        {
          case AXIS_X:
            joint_info += " along X";
            model.addBody(  parent_joint_id, JointModelRX(), jointPlacement, Y,
                          maxEffort, velocity, lowerPosition, upperPosition,
                          joint->name,link->name, has_visual );
            break;
          case AXIS_Y:
            joint_info += " along Y";
            model.addBody(  parent_joint_id, JointModelRY(), jointPlacement, Y,
                          maxEffort, velocity, lowerPosition, upperPosition,
                          joint->name,link->name, has_visual );
            break;
          case AXIS_Z:
            joint_info += " along Z";
            model.addBody(  parent_joint_id, JointModelRZ(), jointPlacement, Y,
                          maxEffort, velocity, lowerPosition, upperPosition,
                          joint->name,link->name, has_visual );
            break;
          case AXIS_UNALIGNED:
          {
            
            std::stringstream axis_value;
            axis_value << std::setprecision(5);
            axis_value << "(" << joint->axis.x <<"," << joint->axis.y << "," << joint->axis.z << ")";
            joint_info += " unaligned " + axis_value.str();
        
            jointAxis = Eigen::Vector3d( joint->axis.x,joint->axis.y,joint->axis.z );
            jointAxis.normalize();
            model.addBody(  parent_joint_id, JointModelRevoluteUnaligned(jointAxis),
                          jointPlacement, Y,
                          maxEffort, velocity, lowerPosition, upperPosition,
                          joint->name,link->name, has_visual );
            break;
          }
          default:
            assert( false && "Fatal Error while extracting revolute joint axis");
            break;
        }
        break;
      }
      case ::urdf::Joint::CONTINUOUS: // Revolute with no joint limits
      {

        joint_info = "joint CONTINUOUS with axis";
        Eigen::VectorXd maxEffort;
        Eigen::VectorXd velocity;
        Eigen::VectorXd lowerPosition;
        Eigen::VectorXd upperPosition;

        if (joint->limits)
        {
          maxEffort.resize(1);      maxEffort     << joint->limits->effort;
          velocity.resize(1);       velocity      << joint->limits->velocity;
          lowerPosition.resize(1);  lowerPosition << joint->limits->lower;
          upperPosition.resize(1);  upperPosition << joint->limits->upper;
        }

        Eigen::Vector3d jointAxis(Eigen::Vector3d::Zero());
        AxisCartesian axis = extractCartesianAxis(joint->axis);
        switch(axis)
        {
          case AXIS_X:
            model.addBody(  parent, JointModelRX(), jointPlacement, Y,
            joint_info += " along X";
                            maxEffort, velocity, lowerPosition, upperPosition,
                            joint->name,link->name, visual );
            break;
          case AXIS_Y:
            model.addBody(  parent, JointModelRY(), jointPlacement, Y,
            joint_info += " along Y";
                            maxEffort, velocity, lowerPosition, upperPosition,
                            joint->name,link->name, visual );
            break;
          case AXIS_Z:
            model.addBody(  parent, JointModelRZ(), jointPlacement, Y,
            joint_info += " along Z";
                            maxEffort, velocity, lowerPosition, upperPosition,
                            joint->name,link->name, visual );
            break;
          case AXIS_UNALIGNED:
          {
            std::stringstream axis_value;
            axis_value << std::setprecision(5);
            axis_value << "(" << joint->axis.x <<"," << joint->axis.y << "," << joint->axis.z << ")";
            joint_info += " unaligned " + axis_value.str();
            
            jointAxis = Eigen::Vector3d( joint->axis.x,joint->axis.y,joint->axis.z );
            jointAxis.normalize();
            model.addBody(  parent, JointModelRevoluteUnaligned(jointAxis), 
                            jointPlacement, Y,
                            maxEffort, velocity, lowerPosition, upperPosition,
                            joint->name,link->name, visual );
            break;
          }
          default:
            assert( false && "Fatal Error while extracting revolute joint axis");
            break;
        }
        break;
      }
      case ::urdf::Joint::PRISMATIC:
      {
        joint_info = "joint PRISMATIC with axis";
        Eigen::VectorXd maxEffort = Eigen::VectorXd(0.);
        Eigen::VectorXd velocity = Eigen::VectorXd(0.);
        Eigen::VectorXd lowerPosition = Eigen::VectorXd(0.);
        Eigen::VectorXd upperPosition = Eigen::VectorXd(0.);

        if (joint->limits)
        {
          maxEffort.resize(1);      maxEffort     << joint->limits->effort;
          velocity.resize(1);       velocity      << joint->limits->velocity;
          lowerPosition.resize(1);  lowerPosition << joint->limits->lower;
          upperPosition.resize(1);  upperPosition << joint->limits->upper;
        }
        AxisCartesian axis = extractCartesianAxis(joint->axis);   
        switch(axis)
        {
          case AXIS_X:
            model.addBody(  parent, JointModelPX(), jointPlacement, Y,
                            maxEffort, velocity, lowerPosition, upperPosition,
                            joint->name,link->name, visual );
            break;
          case AXIS_Y:
            model.addBody(  parent, JointModelPY(), jointPlacement, Y,
                            maxEffort, velocity, lowerPosition, upperPosition,
                            joint->name,link->name, visual );
            break;
          case AXIS_Z:
            model.addBody(  parent, JointModelPZ(), jointPlacement, Y,
                            maxEffort, velocity, lowerPosition, upperPosition,
                            joint->name,link->name, visual );
            break;
          case AXIS_UNALIGNED:
          {
            std::stringstream axis_value;
            axis_value << std::setprecision(5);
            axis_value << "(" << joint->axis.x <<"," << joint->axis.y << "," << joint->axis.z << ")";
            joint_info += " unaligned " + axis_value.str();
            std::cerr << "Bad axis = " << axis_value << std::endl;
            assert(false && "Only X, Y or Z axis are accepted." );
            break;
          }
          default:
            assert( false && "Fatal Error while extracting prismatic joint axis");
            break;
        }
        break;
      }
      case ::urdf::Joint::FIXED:
      {
        // In case of fixed joint, if link has inertial tag:
        //    -add the inertia of the link to his parent in the model
        // Otherwise do nothing.
        // In all cases:
        //    -let all the children become children of parent
        //    -inform the parser of the offset to apply
        //    -add fixed body in model to display it in gepetto-viewer
        if (link->inertial)
        {
          model.mergeFixedBody(parent, jointPlacement, Y); //Modify the parent inertia in the model
        }

        SE3 ptjot_se3 = convertFromUrdf(link->parent_joint->parent_to_joint_origin_transform);

        //transformation of the current placement offset
        nextPlacementOffset = placementOffset*ptjot_se3;

        //add the fixed Body in the model for the viewer
        model.addFixedBody(parent,nextPlacementOffset,link->name,visual);

        BOOST_FOREACH(::urdf::LinkPtr child_link,link->child_links)
        {
          child_link->setParent(link->getParent() );  //skip the fixed generation
        }
        break;
      }
      default:
      {
        std::cerr << "The joint type " << joint->type << " is not supported." << std::endl;
        assert(false && "Only revolute, prismatic and fixed joints are accepted." );
        break;
      }
    }

          if (verbose)
          {
            std::cout << "Adding Body" << std::endl;
            std::cout << "\"" << link_name << "\" connected to " << "\"" << parent_link_name << "\" throw joint " << "\"" << joint_name << "\"" << std::endl;
            std::cout << "joint type: " << joint_info << std::endl;
            std::cout << "joint placement:\n" << jointPlacement;
            std::cout << "body info: " << std::endl;
            std::cout << "  " << "mass: " << Y.mass() << std::endl;
            std::cout << "  " << "lever: " << Y.lever().transpose() << std::endl;
            std::cout << "  " << "inertia elements (Ixx,Iyx,Iyy,Izx,Izy,Izz): " << Y.inertia().data().transpose() << std::endl << std::endl;
          }
          
  }
          else if (link->getParent() != NULL)
          {
            const std::string exception_message (link->name + " - joint information missing.");
            throw std::invalid_argument(exception_message);
          }


  BOOST_FOREACH(::urdf::LinkConstPtr child,link->child_links)
  {
    parseTree(child, model, nextPlacementOffset, verbose);
  }
}

          ///
          /// \brief Parse a tree with a specific root joint linking the model to the environment.
          ///        The function returns an exception as soon as a necessary Inertia or Joint information are missing.
          ///
          /// \param[in] link The current URDF link.
          /// \param[in] model The model where the link must be added.
          /// \param[in] placementOffset The relative placement of the link relative to the closer non fixed joint in the tree.
          /// \param[in] root_joint The specific root joint.
          ///
    template <typename D>
    void parseTree( ::urdf::LinkConstPtr link, Model & model, const SE3 & placementOffset, const JointModelBase<D> & root_joint, const bool verbose = false) throw (std::invalid_argument)
    {
      const Inertia & Y = (link->inertial) ?
      convertFromUrdf(*link->inertial)
      : Inertia::Identity();
      model.addBody( 0, root_joint, placementOffset, Y , "root_joint", link->name, true );
      BOOST_FOREACH(::urdf::LinkConstPtr child,link->child_links)
      {
        parseTree(child, model, SE3::Identity(), verbose);
      }
    }

    ///
    /// \brief Build the model from a URDF file with a particular joint as root of the model tree.
    ///
    /// \param[in] filemane The URDF complete file path.
    /// \param[in] root_joint The joint at the root of the model tree.
    ///
    /// \return The se3::Model of the URDF file.
    ///
    template <typename D>
    Model buildModel(const std::string & filename, const JointModelBase<D> & root_joint, bool verbose = false) throw (std::invalid_argument)
    {
      Model model;

      ::urdf::ModelInterfacePtr urdfTree = ::urdf::parseURDFFile (filename);
      if (urdfTree)
        parseTree(urdfTree->getRoot(), model, SE3::Identity(), root_joint, verbose);
      else
      {
        const std::string exception_message ("The file " + filename + " does not contain a valid URDF model.");
        throw std::invalid_argument(exception_message);
      }
      
      return model;
    }
          
    ///
    /// \brief Build the model from a URDF file with a fixed joint as root of the model tree.
    ///
    /// \param[in] filemane The URDF complete file path.
    ///
    /// \return The se3::Model of the URDF file.
    ///
    inline Model buildModel(const std::string & filename, const bool verbose = false) throw (std::invalid_argument)
    {
      Model model;

      ::urdf::ModelInterfacePtr urdfTree = ::urdf::parseURDFFile (filename);
      if (urdfTree)
        parseTree(urdfTree->getRoot(), model, SE3::Identity(), verbose);
      else
      {
        const std::string exception_message ("The file " + filename + " does not contain a valid URDF model.");
        throw std::invalid_argument(exception_message);
      }

      return model;
    }

  } // namespace urdf
} // namespace se3

#endif // ifndef __se3_urdf_hpp__

