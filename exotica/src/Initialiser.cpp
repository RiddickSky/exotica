/*
 *      Author: Michael Camilleri
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

#include "exotica/Initialiser.h"

using namespace rapidjson;

namespace exotica
{

  Initialiser::Initialiser()
  {
    //!< Empty constructor
  }

  void Initialiser::initialise(const std::string & file_name,
      Server_ptr & server, MotionSolver_ptr & solver,
      PlanningProblem_ptr & problem)
  {
    std::vector<std::string> probs, sols;
    Initialiser::listSolversAndProblems(file_name, probs, sols);
    if (probs.size() > 0 && sols.size() > 0)
    {
        return initialise(file_name, server, solver, problem, probs[0], sols[0]);
    }
    else
    {
        throw_named("No problems or solvers found!");
    }
  }

  void Initialiser::initialiseProblemJSON(PlanningProblem_ptr problem,
      const std::string& constraints)
  {
    {
      Document document;
      if (!document.Parse<0>(constraints.c_str()).HasParseError())
      {
        problem->reinitialise(document, problem);
      }
      else
      {
        throw_named("Can't parse constraints from JSON string!\n"<<document.GetParseError() <<"\n"<<constraints.substr(document.GetErrorOffset(),50));
      }
    }
  }

  void Initialiser::listSolversAndProblems(const std::string & file_name,
      std::vector<std::string>& problems, std::vector<std::string>& solvers)
  {
    xml_file.Clear();
    if (xml_file.LoadFile(file_name.c_str()) != tinyxml2::XML_NO_ERROR)
    {
      xml_file.Clear();
      throw_named("Can't load file!");
    }
    tinyxml2::XMLHandle root_handle(xml_file.RootElement());

    //!< First resolve any includes that exist
    std::string file_path = file_name;
    try
    {
        resolveParent(file_path);
    }
    catch (Exception e)
    {
      xml_file.Clear();
      throw_named("Can't resolve parent!");
    }
    file_path += "/";
    try
    {
        parseIncludes(root_handle, file_path);
    }
    catch (Exception e)
    {
      xml_file.Clear();
      throw_named("Can't parse includes!");
    }

    std::vector<std::string> registered_problems;
    PlanningProblem_fac::Instance().listImplementations(registered_problems);
    tinyxml2::XMLHandle problem_handle(root_handle.FirstChildElement());
    problems.clear();
    while (problem_handle.ToElement())
    {
      const char* atr = problem_handle.ToElement()->Attribute("name");
      const char* tp = problem_handle.ToElement()->Name();
      if (atr)
      {
        bool found = false;
        for (std::string& name : registered_problems)
        {
          if (std::string(tp).compare(name) == 0)
          {
            found = true;
            problems.push_back(std::string(atr));
            break;
          }
        }
        if (!found)
        {
          ERROR("Problem '"<<atr<<"' has unrecognized type '"<<tp<<"'");
        }
        problem_handle = problem_handle.NextSibling();
      }
      else
      {
        problem_handle = problem_handle.NextSibling();
        ERROR("Element name was not specified!");
        continue;
      }
    }

    std::vector<std::string> registered_solvers;
    MotionSolver_fac::Instance().listImplementations(registered_solvers);
    tinyxml2::XMLHandle solver_handle(root_handle.FirstChildElement());
    solvers.clear();
    while (solver_handle.ToElement())
    {
      const char* atr = solver_handle.ToElement()->Attribute("name");
      const char* tp = solver_handle.ToElement()->Name();
      if (atr)
      {
        bool found = false;
        for (std::string& name : registered_solvers)
        {
          if (std::string(tp).compare(name) == 0)
          {
            found = true;
            solvers.push_back(std::string(atr));
            break;
          }
        }
        if (!found)
        {
          ERROR("Solver '"<<atr<<"' has unrecognized type '"<<tp<<"'");
        }
        solver_handle = solver_handle.NextSibling();
      }
      else
      {
        solver_handle = solver_handle.NextSibling();
        ERROR("Element name was not specified!");
        continue;
      }
    }
  }

  void Initialiser::initialise(tinyxml2::XMLHandle root_handle,
      Server_ptr & server)
  {
    server = Server::Instance();
    tinyxml2::XMLHandle server_handle(root_handle.FirstChildElement("Server"));
    if (server_handle.ToElement())
    {
      server->initialise(server_handle);
    }
    else
    {
      throw_named("EXOTica Server element is missing in the xml");
    }
  }

  void Initialiser::initialise(tinyxml2::XMLHandle root_handle,
      PlanningProblem_ptr & problem, const std::string & problem_name,
      Server_ptr & server)
  {
    tinyxml2::XMLHandle problem_handle(root_handle.FirstChildElement());
    while (problem_handle.ToElement())
    {
      const char* atr = problem_handle.ToElement()->Attribute("name");
      if (atr)
      {
        if (std::string(atr).compare(problem_name) == 0)
        {

            PlanningProblem_fac::Instance().createObject(problem,problem_handle, server);
            return;
        }
        else
        {
          problem_handle = problem_handle.NextSibling();
          continue;
        }
      }
      else
      {
        problem_handle = problem_handle.NextSibling();
        ERROR("Element name was not specified!");
        continue;
      }
    }

    throw_named("File does not contain the '"<<problem_name<<"' problem definition.");
  }

  void Initialiser::initialise(tinyxml2::XMLHandle root_handle,
      MotionSolver_ptr & solver, const std::string & solver_name,
      Server_ptr & server)
  {
    tinyxml2::XMLHandle solver_handle(root_handle.FirstChildElement());
    while (solver_handle.ToElement())
    {
      if (!solver_name.empty())
      {
        const char* atr = solver_handle.ToElement()->Attribute("name");
        if (atr)
        {
          if (std::string(atr).compare(solver_name) == 0)
          {
            MotionSolver_fac::Instance().createObject(solver, solver_handle,server);
            return;
          }
          else
          {
            solver_handle = solver_handle.NextSibling();
            continue;
          }

        }
        else
        {
          solver_handle = solver_handle.NextSibling();
          ERROR("Element name was not specified!");
          continue;
        }
      }
      else
      {
        throw_named("Empty solver name");
      }
    }

    throw_named("File does not contain the '"<<solver_name<<"' solver.");
  }

  void Initialiser::initialise(const std::string & file_name,
      Server_ptr & server, MotionSolver_ptr & solver,
      PlanningProblem_ptr & problem, const std::string & problem_name,
      const std::string & solver_name)
  {
    // /////////////////////////
    // Create the document
    xml_file.Clear();
    if (xml_file.LoadFile(file_name.c_str()) != tinyxml2::XML_NO_ERROR)
    {
      xml_file.Clear();
      throw_named("Can't load XML!");
    }
    tinyxml2::XMLHandle root_handle(xml_file.RootElement());

    // /////////////////////////
    // First resolve any includes that exist
    std::string file_path = file_name;
    try
    {
        resolveParent(file_path);
    }
    catch(Exception e)
    {
      xml_file.Clear();
      throw_named("Can't resolve parent!");
    }
    file_path += "/";
    try
    {
        parseIncludes(root_handle, file_path);
    }
    catch (Exception e)
    {
      xml_file.Clear();
      throw_named("Can't parse includes!");
    }

    // /////////////////////////
    // Initialise server
    initialise(root_handle, server);

    // /////////////////////////
    // Initialise problem
    initialise(root_handle, problem, problem_name, server);

    // /////////////////////////
    // Initialise solver
    initialise(root_handle, solver, solver_name, server);
  }

  void Initialiser::initialise(const std::string & file_name,
      Server_ptr & server, std::vector<MotionSolver_ptr> & solver,
      std::vector<PlanningProblem_ptr> & problem,
      std::vector<std::string> & problem_name,
      std::vector<std::string> & solver_name)
  {
    // /////////////////////////
    // Create the document
    xml_file.Clear();
    if (xml_file.LoadFile(file_name.c_str()) != tinyxml2::XML_NO_ERROR)
    {
      xml_file.Clear();
      throw_named("Can't load XML!");
    }
    tinyxml2::XMLHandle root_handle(xml_file.RootElement());

    // /////////////////////////
    // First resolve any includes that exist
    std::string file_path = file_name;
    try
    {
        resolveParent(file_path);
    }
    catch(Exception e)
    {
      xml_file.Clear();
      throw_named("Can't resolve parent!");
    }
    file_path += "/";
    try
    {
        parseIncludes(root_handle, file_path);
    }
    catch (Exception e)
    {
      xml_file.Clear();
      throw_named("Can't parse includes!");
    }

    // /////////////////////////
    // Initialise server
    initialise(root_handle, server);

    // /////////////////////////
    // Initialise problem
    problem.resize(problem_name.size());
    for (int i = 0; i < problem_name.size(); i++)
    {
      initialise(root_handle, problem[i], problem_name[i], server);
    }

    // /////////////////////////
    // Initialise solver
    solver.resize(solver_name.size());
    for (int i = 0; i < solver_name.size(); i++)
    {
      initialise(root_handle, solver[i], solver_name[i], server);
    }
  }
}
