/*
 *  Created on: 19 Jun 2014
 *      Author: Vladimir Ivan
 * 
 * Copyright (c) 2016, University Of Edinburgh 
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met: 
 * 
 *  * Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer. 
 *  * Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *  * Neither the name of  nor the names of its contributors may be used to 
 *    endorse or promote products derived from this software without specific 
 *    prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "ompl_solver/OMPLProblem.h"
#include "Identity.h"

REGISTER_PROBLEM_TYPE("OMPLProblem", exotica::OMPLProblem);
REGISTER_TASKDEFINITION_TYPE("TaskBias", exotica::TaskBias);

#define XML_CHECK(x) {xmltmp=handle.FirstChildElement(x).ToElement();if (!xmltmp) throw_named("XML element '"<<x<<"' does not exist!");}

namespace exotica
{

  TaskBias::TaskBias()
      : TaskSqrError()
  {

  }

  OMPLProblem::OMPLProblem()
      : space_dim_(0), problemType(OMPL_PROBLEM_GOAL)
  {
    // TODO Auto-generated constructor stub

  }

  OMPLProblem::~OMPLProblem()
  {
    // TODO Auto-generated destructor stub
  }

  std::vector<double>& OMPLProblem::getBounds()
  {
    return bounds_;
  }

  void OMPLProblem::clear(bool keepOriginals)
  {
    if (keepOriginals)
    {
      task_maps_ = originalMaps_;
      goals_ = originalGoals_;
      costs_ = originalCosts_;
      goalBias_ = originalGoalBias_;
      samplingBias_ = originalSamplingBias_;
    }
    else
    {
      task_maps_.clear();
      task_defs_.clear();
      goals_.clear();
      costs_.clear();
      goalBias_.clear();
      samplingBias_.clear();
    }
  }

  void OMPLProblem::reinitialise(rapidjson::Document& document,
      boost::shared_ptr<PlanningProblem> problem)
  {
    clear();
    if (document.IsArray())
    {
      for (rapidjson::SizeType i = 0; i < document.Size(); i++)
      {
        rapidjson::Value& obj = document[i];
        if (obj.IsObject())
        {
          std::string constraintClass;
          getJSON(obj["class"], constraintClass);
          if (knownMaps_.find(constraintClass) != knownMaps_.end())
          {
              bool IsGoal = false;
              if (knownMaps_[constraintClass].compare("Identity") == 0)
              {
                std::string postureName;
                try
                {
                    getJSON(obj["postureName"], postureName);
                  if (postureName.compare("reach_end") != 0)
                  {
                    continue;
                  }
                  else
                  {
                    IsGoal = true;
                  }
                }
                catch (Exception e)
                {
                  continue;
                }
              }
                  if (problemType == OMPL_PROBLEM_GOAL_BIAS && !IsGoal) continue;
                  TaskMap_ptr taskmap;
                  TaskMap_fac::Instance().createObject(knownMaps_[constraintClass], taskmap);
                  taskmap->initialise(obj, server_, scenes_,problem);
                  std::string name = taskmap->getObjectName();
                  task_maps_[name] = taskmap;
                  TaskDefinition_ptr task;
                  TaskDefinition_fac::Instance().createObject("TaskTerminationCriterion", task);
                  TaskTerminationCriterion_ptr sqr =boost::static_pointer_cast<TaskTerminationCriterion>(task);
                  sqr->setTaskMap(taskmap);
                  int dim;
                  taskmap->taskSpaceDim(dim);
                  sqr->y_star0_.resize(dim);
                  sqr->rho0_(0) = 1.0;
                  sqr->threshold0_(0) = 1e-6;
                  sqr->object_name_ = name+ std::to_string((unsigned long) sqr.get());

                  // TODO: Better implementation of stting goals from JSON
                  sqr->y_star0_.setZero();

                  sqr->setTimeSteps(1);
                  sqr->wasFullyInitialised_ = true;
                  task_defs_[name] = task;
                  goals_.push_back(sqr);

            }
            else
            {
//              WARNING("Ignoring unknown constraint '"<<constraintClass<<"'");
            }
          }
          else
          {
            throw_named("Invalid JSON document object!");
          }
      }

      // HACK - special case
      // Add goal bias ////////////////////////////
      if (!problem->endStateName.empty()
          && problemType == OMPL_PROBLEM_GOAL_BIAS)
      {
        TaskMap_ptr taskmap;
        TaskMap_fac::Instance().createObject("Identity", taskmap);
          boost::shared_ptr<exotica::Identity> idt = boost::static_pointer_cast<
              exotica::Identity>(taskmap);
          std::vector<std::pair<std::string,std::string> > tmpParams;
          taskmap->initialiseManual("exotica::Identity", server_,scenes_, problem,tmpParams);
          idt->initialise(problem->endStateName,*(problem->posesJointNames));
              std::string name = taskmap->getObjectName();
              task_maps_[name] = taskmap;
              TaskDefinition_ptr task;
              if (endState.rows() > 0)
              {
                TaskDefinition_fac::Instance().createObject("TaskBias",task);
                  TaskBias_ptr sqr = boost::static_pointer_cast<TaskBias>(task);
                  sqr->setTaskMap(taskmap);
                  int dim;
                  taskmap->taskSpaceDim(dim);
                  sqr->y_star0_.resize(dim);
                  sqr->rho0_(0) = 1.0;
                  sqr->object_name_ = name
                      + std::to_string((unsigned long) sqr.get());
                  sqr->y_star0_.setZero();

                  sqr->setTimeSteps(1);
                  sqr->wasFullyInitialised_ = true;
                  task_defs_[name] = task;
                  goalBias_.push_back(sqr);
              }
      }

    }
    else
    {
        throw_named("Invalid JSON array!");
    }
    std::vector<std::string> jnts;
    scenes_.begin()->second->getJointNames(jnts);
    getBounds().resize(jnts.size() * 2);

    if (scenes_.begin()->second->getBaseType() == BASE_TYPE::FLOATING
        && server_->hasParam(server_->getName() + "/FloatingBaseLowerLimits")
        && server_->hasParam(server_->getName() + "/FloatingBaseUpperLimits"))
    {

      EParam<exotica::Vector> tmp_lower, tmp_upper;
      server_->getParam(server_->getName() + "/FloatingBaseLowerLimits",tmp_lower);
      server_->getParam(server_->getName() + "/FloatingBaseUpperLimits",tmp_upper);
      if (tmp_lower->data.size() == 6 && tmp_upper->data.size() == 6)
      {
        std::vector<double> lower = tmp_lower->data;
        std::vector<double> upper = tmp_upper->data;
        for (int i = 0; i < 3; i++)
        {
          lower[i] += std::min((double) startState(i), (double) endState(i));
          upper[i] += std::max((double) startState(i), (double) endState(i));
        }
        scenes_.begin()->second->getSolver().setFloatingBaseLimitsPosXYZEulerZYX(
            lower, upper);
      }
      else
      {
        throw_named("Can't register parameters!");
      }
    }
    else if (scenes_.begin()->second->getBaseType() == BASE_TYPE::FLOATING)
    {
      WARNING("Using floating base without bounds!");
    }

    std::map<std::string, std::vector<double>> joint_limits =
        scenes_.begin()->second->getSolver().getUsedJointLimits();
    for (int i = 0; i < 3; i++)
    {
      getBounds()[i] = joint_limits.at(jnts[i])[0];
      getBounds()[i + jnts.size()] = joint_limits.at(jnts[i])[1];
    }
    for (int i = 6; i < jnts.size(); i++)
    {
      getBounds()[i] = joint_limits.at(jnts[i])[0] - 1e-3;
      getBounds()[i + jnts.size()] = joint_limits.at(jnts[i])[1] + 1e-3;
    }
  }

  void OMPLProblem::initDerived(tinyxml2::XMLHandle & handle)
  {
    tinyxml2::XMLHandle tmp_handle = handle.FirstChildElement("PlroblemType");
    if (tmp_handle.ToElement())
    {
      std::string tmp = tmp_handle.ToElement()->GetText();
      if (tmp.compare("Costs") == 0)
      {
        problemType = OMPL_PROBLEM_COSTS;
      }
      else if (tmp.compare("GoalBias") == 0)
      {
        problemType = OMPL_PROBLEM_GOAL_BIAS;
      }
      else if (tmp.compare("SamplingBias") == 0)
      {
        problemType = OMPL_PROBLEM_SAMPLING_BIAS;
      }
      else if (tmp.compare("Goals") == 0)
      {
        problemType = OMPL_PROBLEM_GOAL;
      }
      else
      {
        throw_named("Unknown problem type!");
      }
    }
    else
    {
      problemType = OMPL_PROBLEM_GOAL;
    }

    switch (problemType)
    {
    case OMPL_PROBLEM_GOAL:
      for (auto goal : task_defs_)
      {
        if (goal.second->type().compare("exotica::TaskTerminationCriterion")
            == 0)
        {
          goals_.push_back(
              boost::static_pointer_cast<exotica::TaskTerminationCriterion>(
                  goal.second));
        }
        else
        {
          ERROR(goal.first << " has wrong type, ignored!");
        }
      }
      break;

    case OMPL_PROBLEM_COSTS:
      for (auto goal : task_defs_)
      {
        if (goal.second->type().compare("exotica::TaskSqrError") == 0)
        {
          costs_.push_back(
              boost::static_pointer_cast<exotica::TaskSqrError>(goal.second));
        }
        else
        {
          ERROR(goal.first << " has wrong type, ignored!");
        }
      }
      break;

    case OMPL_PROBLEM_GOAL_BIAS:
      for (auto goal : task_defs_)
      {
        if (goal.second->type().compare("exotica::TaskBias") == 0)
        {
          goalBias_.push_back(
              boost::static_pointer_cast<exotica::TaskBias>(goal.second));
        }
        else
        {
          ERROR(goal.first << " has wrong type, ignored!");
        }
      }
      break;

    case OMPL_PROBLEM_SAMPLING_BIAS:
      for (auto goal : task_defs_)
      {
        if (goal.second->type().compare("exotica::TaskBias") == 0)
        {
          samplingBias_.push_back(
              boost::static_pointer_cast<exotica::TaskBias>(goal.second));
        }
        else
        {
          ERROR(goal.first << " has wrong type, ignored!");
        }
      }
      break;
    }

    tmp_handle = handle.FirstChildElement("LocalPlannerConfig");
    if (tmp_handle.ToElement())
    {
      local_planner_config_ = tmp_handle.ToElement()->GetText();
    }

    for (auto scene : scenes_)
    {
      int nn = scene.second->getNumJoints();
      if (space_dim_ == 0)
      {
        space_dim_ = nn;
        continue;
      }
      else
      {
        if (space_dim_ != nn)
        {
          throw_named("Kinematic scenes have different joint space sizes!");
        }
        else
        {
          continue;
        }
      }
    }

    originalMaps_ = task_maps_;
    originalGoals_ = goals_;
    originalCosts_ = costs_;
    originalGoalBias_ = goalBias_;
    originalSamplingBias_ = samplingBias_;

    if (scenes_.begin()->second->getBaseType() != exotica::BASE_TYPE::FIXED)
      compound_ = true;
    else
      compound_ = false;
    std::vector<std::string> jnts;
    scenes_.begin()->second->getJointNames(jnts);

    getBounds().resize(jnts.size() * 2);
    std::map<std::string, std::vector<double>> joint_limits =
        scenes_.begin()->second->getSolver().getUsedJointLimits();
    for (int i = 0; i < jnts.size(); i++)
    {
      getBounds()[i] = joint_limits.at(jnts[i])[0];
      getBounds()[i + jnts.size()] = joint_limits.at(jnts[i])[1];
    }
  }

  int OMPLProblem::getSpaceDim()
  {
    return space_dim_;
  }

  std::vector<TaskTerminationCriterion_ptr>& OMPLProblem::getGoals()
  {
    return goals_;
  }

  bool OMPLProblem::isCompoundStateSpace()
  {
    return compound_;
  }
  std::vector<TaskSqrError_ptr>& OMPLProblem::getCosts()
  {
    return costs_;
  }

  std::vector<TaskBias_ptr>& OMPLProblem::getGoalBias()
  {
    return goalBias_;
  }

  std::vector<TaskBias_ptr>& OMPLProblem::getSamplingBias()
  {
    return samplingBias_;
  }

} /* namespace exotica */
