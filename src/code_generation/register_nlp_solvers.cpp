/*
 *    This file is part of ACADO Toolkit.
 *
 *    ACADO Toolkit -- A Toolkit for Automatic Control and Dynamic Optimization.
 *    Copyright (C) 2008-2014 by Boris Houska, Hans Joachim Ferreau,
 *    Milan Vukov, Rien Quirynen, KU Leuven.
 *    Developed within the Optimization in Engineering Center (OPTEC)
 *    under supervision of Moritz Diehl. All rights reserved.
 *
 *    ACADO Toolkit is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    ACADO Toolkit is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with ACADO Toolkit; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 *    \file src/code_generation/register_nlp_solvers.cpp
 *    \author Milan Vukov
 *    \date 2013
 */

#include <acado/code_generation/register_nlp_solvers.hpp>

#include <acado/code_generation/export_gauss_newton_cn2.hpp>
#include <acado/code_generation/export_gauss_newton_cn2_new.hpp>
#include <acado/code_generation/export_gauss_newton_cn2_factorization.hpp>
#include <acado/code_generation/export_gauss_newton_condensed.hpp>
#include <acado/code_generation/export_gauss_newton_forces.hpp>
#include <acado/code_generation/export_gauss_newton_qpdunes.hpp>
#include <acado/code_generation/export_gauss_newton_hpmpc.hpp>

BEGIN_NAMESPACE_ACADO

//
// Solver registration
//

ExportNLPSolver* createGaussNewtonCN2(	UserInteraction* _userInteraction,
										const std::string& _commonHeaderName
										)
{
	return new ExportGaussNewtonCN2(_userInteraction, _commonHeaderName);
}

ExportNLPSolver* createGaussNewtonCN2New(	UserInteraction* _userInteraction,
											const std::string& _commonHeaderName
											)
{
	return new ExportGaussNewtonCN2New(_userInteraction, _commonHeaderName);
}

ExportNLPSolver* createGaussNewtonCn2Factorization(	UserInteraction* _userInteraction,
													const std::string& _commonHeaderName
													)
{
	return new ExportGaussNewtonCn2Factorization(_userInteraction, _commonHeaderName);
}

ExportNLPSolver* createGaussNewtonCondensed(	UserInteraction* _userInteraction,
												const std::string& _commonHeaderName
												)
{
	return new ExportGaussNewtonCondensed(_userInteraction, _commonHeaderName);
}

ExportNLPSolver* createGaussNewtonForces(	UserInteraction* _userInteraction,
											const std::string& _commonHeaderName
											)
{
	return new ExportGaussNewtonForces(_userInteraction, _commonHeaderName);
}

ExportNLPSolver* createGaussNewtonQpDunes(	UserInteraction* _userInteraction,
											const std::string& _commonHeaderName
											)
{
	return new ExportGaussNewtonQpDunes(_userInteraction, _commonHeaderName);
}

ExportNLPSolver* createGaussNewtonHpmpc(	UserInteraction* _userInteraction,
											const std::string& _commonHeaderName
										)
{
	return new ExportGaussNewtonHpmpc(_userInteraction, _commonHeaderName);
}

RegisterNlpSolvers::RegisterNlpSolvers()
{
	NLPSolverFactory::instance().registerAlgorithm(GAUSS_NEWTON_CN2, createGaussNewtonCN2);
	NLPSolverFactory::instance().registerAlgorithm(GAUSS_NEWTON_CN2_NEW, createGaussNewtonCN2New);
	NLPSolverFactory::instance().registerAlgorithm(GAUSS_NEWTON_CN2_FACTORIZATION, createGaussNewtonCn2Factorization);
	NLPSolverFactory::instance().registerAlgorithm(GAUSS_NEWTON_CONDENSED, createGaussNewtonCondensed);
	NLPSolverFactory::instance().registerAlgorithm(GAUSS_NEWTON_FORCES, createGaussNewtonForces);
	NLPSolverFactory::instance().registerAlgorithm(GAUSS_NEWTON_HPMPC, createGaussNewtonHpmpc);
}

CLOSE_NAMESPACE_ACADO
