//
// Copyright (c) 2015-2018 CNRS
//

#include "pinocchio/bindings/python/algorithm/algorithms.hpp"
#include "pinocchio/algorithm/jacobian.hpp"

namespace pinocchio
{
  namespace python
  {
    
    static Data::Matrix6x
    jacobian_proxy(const Model & model,
                   Data & data,
                   const Eigen::VectorXd & q,
                   Model::JointIndex jointId,
                   ReferenceFrame rf,
                   bool update_kinematics)
    {
      Data::Matrix6x J(6,model.nv); J.setZero();
      
      if (update_kinematics)
        computeJointJacobians(model,data,q);
      
      getJointJacobian(model,data,jointId,rf,J);
      
      return J;
    }
    
    static Data::Matrix6x
    get_jacobian_proxy(const Model & model,
                       Data & data,
                       Model::JointIndex jointId,
                       ReferenceFrame rf)
    {
      Data::Matrix6x J(6,model.nv); J.setZero();
      getJointJacobian(model,data,jointId,rf,J);
      
      return J;
    }
    
    static Data::Matrix6x
    get_jacobian_time_variation_proxy(const Model & model,
                                  Data & data,
                                  Model::JointIndex jointId,
                                  ReferenceFrame rf)
    {
      Data::Matrix6x dJ(6,model.nv); dJ.setZero();
      getJointJacobianTimeVariation(model,data,jointId,rf,dJ);
      
      return dJ;
    }
  
    void exposeJacobian()
    {
      using namespace Eigen;
      
      bp::def("computeJointJacobians",
              &computeJointJacobians<double,0,JointCollectionDefaultTpl,VectorXd>,
              bp::args("Model","Data",
                       "Joint configuration q (size Model::nq)"),
              "Computes the full model Jacobian, i.e. the stack of all motion subspace expressed in the world frame.\n"
              "The result is accessible through data.J. This function computes also the forwardKinematics of the model.",
              bp::return_value_policy<bp::return_by_value>());

      bp::def("computeJointJacobians",
              &computeJointJacobians<double,0,JointCollectionDefaultTpl>,
              bp::args("Model","Data"),
              "Computes the full model Jacobian, i.e. the stack of all motion subspace expressed in the world frame.\n"
              "The result is accessible through data.J. This function assumes that forwardKinematics has been called before",
              bp::return_value_policy<bp::return_by_value>());
      
      bp::def("jointJacobian",jacobian_proxy,
              bp::args("Model, the model of the kinematic tree",
                       "Data, the data associated to the model where the results are stored",
                       "Joint configuration q (size Model::nq)",
                       "Joint ID, the index of the joint.",
                       "Reference frame rf (either ReferenceFrame.LOCAL or ReferenceFrame.WORLD)",
                       "update_kinematics (true = update the value of the total jacobian)"),
              "Computes the jacobian of a given given joint according to the given input configuration."
              "If rf is set to LOCAL, it returns the jacobian associated to the joint frame. Otherwise, it returns the jacobian of the frame coinciding with the world frame.");

      bp::def("getJointJacobian",get_jacobian_proxy,
              bp::args("Model, the model of the kinematic tree",
                       "Data, the data associated to the model where the results are stored",
                       "Joint ID, the index of the joint.",
                       "Reference frame rf (either ReferenceFrame.LOCAL or ReferenceFrame.WORLD)"),
              "Computes the jacobian of a given given joint according to the given entries in data."
              "If rf is set to LOCAL, it returns the jacobian associated to the joint frame. Otherwise, it returns the jacobian of the frame coinciding with the world frame.");
      
      bp::def("computeJointJacobiansTimeVariation",computeJointJacobiansTimeVariation<double,0,JointCollectionDefaultTpl,VectorXd,VectorXd>,
              bp::args("Model","Data",
                       "Joint configuration q (size Model::nq)",
                       "Joint velocity v (size Model::nv)"),
              "Computes the full model Jacobian variations with respect to time. It corresponds to dJ/dt which depends both on q and v. It also computes the joint Jacobian of the model (similar to computeJointJacobians)."
              "The result is accessible through data.dJ and data.J.",
              bp::return_value_policy<bp::return_by_value>());
      
      bp::def("getJointJacobianTimeVariation",get_jacobian_time_variation_proxy,
              bp::args("Model, the model of the kinematic tree",
                       "Data, the data associated to the model where the results are stored",
                       "Joint ID, the index of the joint.",
                       "Reference frame rf (either ReferenceFrame.LOCAL or ReferenceFrame.WORLD)"),
              "Computes the Jacobian time variation of a specific joint expressed either in the world frame or in the local frame of the joint."
              "You have to call computeJointJacobiansTimeVariation first."
              "If rf is set to LOCAL, it returns the jacobian time variation associated to the joint frame. Otherwise, it returns the jacobian time variation of the frame coinciding with the world frame.");
    }
    
  } // namespace python
} // namespace pinocchio
