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
 *    \file src/code_generation/export_gauss_newton_generic.cpp
 *    \author Rien Quirynen
 *    \date 2017
 */

#include <acado/code_generation/export_gauss_newton_generic.hpp>

BEGIN_NAMESPACE_ACADO

using namespace std;

ExportGaussNewtonGeneric::ExportGaussNewtonGeneric(	UserInteraction* _userInteraction,
													const std::string& _commonHeaderName
													) : ExportNLPSolver( _userInteraction,_commonHeaderName )
{}

returnValue ExportGaussNewtonGeneric::setup( )
{
	setupInitialization();

	setupVariables();

	setupSimulation();

	setupObjectiveEvaluation();

	setupConstraintsEvaluation();

	setupEvaluation();

	setupAuxiliaryFunctions();

	int useArrivalCost;
	get(CG_USE_ARRIVAL_COST, useArrivalCost);
	if (useArrivalCost == YES) {
	    ExportVariable evRet("ret", 1, 1, INT, ACADO_LOCAL, true);

	    ExportVariable evReset("reset", 1, 1, INT, ACADO_LOCAL, true);
	    evReset.setDoc("Reset S_{AC}. Set it to 1 to initialize arrival cost calculation, "
	            "and later should set it to 0.");

	    updateArrivalCost.init("updateArrivalCost", evReset);
	    updateArrivalCost.doc("Use this function to update the arrival cost.");
	    updateArrivalCost.setReturnValue( evRet );
	    updateArrivalCost << (evRet == 0);

	    acWL.setup("WL", NX, NX, REAL, ACADO_VARIABLES);
	    acWL.setDoc("Arrival cost term: Cholesky decomposition, lower triangular, "
	                " of the inverse of the state noise covariance matrix.");
	}

	return SUCCESSFUL_RETURN;
}

returnValue ExportGaussNewtonGeneric::getDataDeclarations(	ExportStatementBlock& declarations,
															ExportStruct dataStruct
															) const
{
	returnValue status;
	status = ExportNLPSolver::getDataDeclarations(declarations, dataStruct);
	if (status != SUCCESSFUL_RETURN)
		return status;

    int hardcodeConstraintValues;
    get(CG_HARDCODE_CONSTRAINT_VALUES, hardcodeConstraintValues);

	declarations.addDeclaration(x0, dataStruct);

	if (Q1.isGiven() == true)
		declarations.addDeclaration(qpQ, dataStruct);
	if (QN1.isGiven() == true)
		declarations.addDeclaration(qpQf, dataStruct);
	if (S1.isGiven() == true)
		declarations.addDeclaration(qpS, dataStruct);
	if (R1.isGiven() == true)
		declarations.addDeclaration(qpR, dataStruct);

	declarations.addDeclaration(qpq, dataStruct);
	declarations.addDeclaration(qpqf, dataStruct);
	declarations.addDeclaration(qpr, dataStruct);

	declarations.addDeclaration(qpx, dataStruct);
	declarations.addDeclaration(qpu, dataStruct);

	declarations.addDeclaration(qpLb, dataStruct);
	declarations.addDeclaration(qpUb, dataStruct);

	declarations.addDeclaration(qpLbA, dataStruct);
	declarations.addDeclaration(qpUbA, dataStruct);

	declarations.addDeclaration(sigmaN, dataStruct);

	declarations.addDeclaration(qpLambda, dataStruct);
	declarations.addDeclaration(qpMu, dataStruct);

	declarations.addDeclaration(nIt, dataStruct);

	if (hardcodeConstraintValues == NO) {
	    declarations.addDeclaration(evLbValues, dataStruct);
        declarations.addDeclaration(evUbValues, dataStruct);

        declarations.addDeclaration(evLbAValues, dataStruct);
        declarations.addDeclaration(evUbAValues, dataStruct);
	}

	return SUCCESSFUL_RETURN;
}

returnValue ExportGaussNewtonGeneric::getFunctionDeclarations(	ExportStatementBlock& declarations
																) const
{
	declarations.addDeclaration( preparation );
	declarations.addDeclaration( feedback );

	declarations.addDeclaration( initialize );
	declarations.addDeclaration( initializeNodes );
	declarations.addDeclaration( shiftStates );
	declarations.addDeclaration( shiftControls );
	declarations.addDeclaration( getKKT );
	declarations.addDeclaration( getObjective );

	declarations.addDeclaration( evaluateStageCost );
	declarations.addDeclaration( evaluateTerminalCost );

    declarations.addDeclaration( updateArrivalCost );

	return SUCCESSFUL_RETURN;
}

returnValue ExportGaussNewtonGeneric::getCode(	ExportStatementBlock& code
												)
{
	string moduleName;
	get(CG_MODULE_NAME, moduleName);

	code.addLinebreak( 2 );
	code.addStatement( "/******************************************************************************/\n" );
	code.addStatement( "/*                                                                            */\n" );
	code.addStatement( "/* ACADO code generation                                                      */\n" );
	code.addStatement( "/*                                                                            */\n" );
	code.addStatement( "/******************************************************************************/\n" );
	code.addLinebreak( 2 );

	int useOMP;
	get(CG_USE_OPENMP, useOMP);
	if ( useOMP )
	{
		code.addDeclaration( state );
	}

	code.addFunction( modelSimulation );

	code.addFunction( evaluateStageCost );
	code.addFunction( evaluateTerminalCost );
	code.addFunction( setObjQ1Q2 );
	code.addFunction( setObjR1R2 );
	code.addFunction( setObjS1 );
	code.addFunction( setObjQN1QN2 );
	code.addFunction( setStagef );
	code.addFunction( evaluateObjective );

	code.addFunction( evaluatePathConstraints );

	for (unsigned i = 0; i < evaluatePointConstraints.size(); ++i)
	{
		if (evaluatePointConstraints[ i ] == 0)
			continue;
		code.addFunction( *evaluatePointConstraints[ i ] );
	}

	code.addFunction( setStagePac );
	code.addFunction( evaluateConstraints );

	code.addFunction( acc );

	code.addFunction( preparation );
	code.addFunction( feedback );

	code.addFunction( initialize );
	code.addFunction( initializeNodes );
	code.addFunction( shiftStates );
	code.addFunction( shiftControls );
	code.addFunction( getKKT );
	code.addFunction( getObjective );

	int useArrivalCost;
	get(CG_USE_ARRIVAL_COST, useArrivalCost);
	if (useArrivalCost == YES) {
	    code.addFunction( updateArrivalCost );
	}

	return SUCCESSFUL_RETURN;
}


unsigned ExportGaussNewtonGeneric::getNumQPvars( ) const
{
	if (initialStateFixed() == true)
		return N * NX + N * NU;

	return (N + 1) * NX + N * NU;
}

//
// PROTECTED FUNCTIONS:
//

returnValue ExportGaussNewtonGeneric::setupObjectiveEvaluation( void )
{
	evaluateObjective.setup("evaluateObjective");

	int variableObjS;
	get(CG_USE_VARIABLE_WEIGHTING_MATRIX, variableObjS);

	ExportVariable evLmX = zeros<double>(NX, NX);
	ExportVariable evLmU = zeros<double>(NU, NU);

	if (levenbergMarquardt > 0.0)
	{
		DMatrix lmX = eye<double>( NX );
		lmX *= levenbergMarquardt;

		DMatrix lmU = eye<double>( NU );
		lmU *= levenbergMarquardt;

		evLmX = lmX;
		evLmU = lmU;
	}

	//
	// Main loop that calculates Hessian and gradients
	//

	ExportIndex runObj( "runObj" );
	ExportForLoop loopObjective( runObj, 0, N );

	evaluateObjective.addIndex( runObj );

	loopObjective.addStatement( objValueIn.getCols(0, getNX()) == x.getRow( runObj ) );
	loopObjective.addStatement( objValueIn.getCols(NX, NX + NU) == u.getRow( runObj ) );
	loopObjective.addStatement( objValueIn.getCols(NX + NU, NX + NU + NOD) == od.getRow( runObj ) );
	loopObjective.addLinebreak( );

	// Evaluate the objective function
	loopObjective.addFunctionCall(evaluateStageCost, objValueIn, objValueOut);

	// Stack the measurement function value
	loopObjective.addStatement(
			Dy.getRows(runObj * NY, (runObj + 1) * NY) ==  objValueOut.getTranspose().getRows(0, getNY())
	);
	loopObjective.addLinebreak( );

	// Optionally compute derivatives

	ExportVariable tmpObjS, tmpFx, tmpFu;
	ExportVariable tmpFxEnd, tmpObjSEndTerm;
	tmpObjS.setup("tmpObjS", NY, NY, REAL, ACADO_LOCAL);
	if (objS.isGiven() == true)
		tmpObjS = objS;
	tmpFx.setup("tmpFx", NY, NX, REAL, ACADO_LOCAL);
	if (objEvFx.isGiven() == true)
		tmpFx = objEvFx;
	tmpFu.setup("tmpFu", NY, NU, REAL, ACADO_LOCAL);
	if (objEvFu.isGiven() == true)
		tmpFu = objEvFu;
	tmpFxEnd.setup("tmpFx", NYN, NX, REAL, ACADO_LOCAL);
	if (objEvFxEnd.isGiven() == true)
		tmpFxEnd = objEvFxEnd;
	tmpObjSEndTerm.setup("tmpObjSEndTerm", NYN, NYN, REAL, ACADO_LOCAL);
	if (objSEndTerm.isGiven() == true)
		tmpObjSEndTerm = objSEndTerm;

	unsigned indexX = getNY();
	ExportArgument tmpFxCall = tmpFx;
	if (tmpFx.isGiven() == false)
	{
		tmpFxCall = objValueOut.getAddress(0, indexX);
		indexX += objEvFx.getDim();
	}

	ExportArgument tmpFuCall = tmpFu;
	if (tmpFu.isGiven() == false)
	{
		tmpFuCall = objValueOut.getAddress(0, indexX);
	}

	ExportArgument objSCall = variableObjS == true ? objS.getAddress(runObj * NY, 0) : objS;

	//
	// Optional computation of Q1, Q2
	//
	if (Q1.isGiven() == false)
	{
		ExportVariable tmpQ1, tmpQ2;
		tmpQ1.setup("tmpQ1", NX, NX, REAL, ACADO_LOCAL);
		tmpQ2.setup("tmpQ2", NX, NY, REAL, ACADO_LOCAL);

		setObjQ1Q2.setup("setObjQ1Q2", tmpFx, tmpObjS, tmpQ1, tmpQ2);
		setObjQ1Q2.addStatement( tmpQ2 == (tmpFx ^ tmpObjS) );
		setObjQ1Q2.addStatement( tmpQ1 == tmpQ2 * tmpFx );
		setObjQ1Q2.addStatement( tmpQ1 += evLmX );

		loopObjective.addFunctionCall(
				setObjQ1Q2,
				tmpFxCall, objSCall,
				Q1.getAddress(runObj * NX, 0), Q2.getAddress(runObj * NX, 0)
		);

		loopObjective.addLinebreak( );
	}
	else if (levenbergMarquardt > 0.0)
		Q1 = Q1.getGivenMatrix() + evLmX.getGivenMatrix();

	if (R1.isGiven() == false)
	{
		ExportVariable tmpR1, tmpR2;
		tmpR1.setup("tmpR1", NU, NU, REAL, ACADO_LOCAL);
		tmpR2.setup("tmpR2", NU, NY, REAL, ACADO_LOCAL);

		setObjR1R2.setup("setObjR1R2", tmpFu, tmpObjS, tmpR1, tmpR2);
		setObjR1R2.addStatement( tmpR2 == (tmpFu ^ tmpObjS) );
		setObjR1R2.addStatement( tmpR1 == tmpR2 * tmpFu );
		setObjR1R2.addStatement( tmpR1 += evLmU );

		loopObjective.addFunctionCall(
				setObjR1R2,
				tmpFuCall, objSCall,
				R1.getAddress(runObj * NU, 0), R2.getAddress(runObj * NU, 0)
		);

		loopObjective.addLinebreak( );
	}
	else if (levenbergMarquardt > 0.0)
		R1 = R1.getGivenMatrix() + evLmU.getGivenMatrix();

	if (S1.isGiven() == false)
	{
		ExportVariable tmpS1;
		ExportVariable tmpS2;

		tmpS1.setup("tmpS1", NX, NU, REAL, ACADO_LOCAL);
		tmpS2.setup("tmpS2", NX, NY, REAL, ACADO_LOCAL);

		setObjS1.setup("setObjS1", tmpFx, tmpFu, tmpObjS, tmpS1);
		setObjS1.addVariable( tmpS2 );
		setObjS1.addStatement( tmpS2 == (tmpFx ^ tmpObjS) );
		setObjS1.addStatement( tmpS1 == tmpS2 * tmpFu );

		loopObjective.addFunctionCall(
				setObjS1,
				tmpFxCall, tmpFuCall, objSCall,
				S1.getAddress(runObj * NX, 0)
		);
	}

	evaluateObjective.addStatement( loopObjective );

	//
	// Evaluate the quadratic Mayer term
	//
	evaluateObjective.addStatement( objValueIn.getCols(0, NX) == x.getRow( N ) );
	evaluateObjective.addStatement( objValueIn.getCols(NX, NX + NOD) == od.getRow( N ) );

	// Evaluate the objective function, last node.
	evaluateObjective.addFunctionCall(evaluateTerminalCost, objValueIn, objValueOut);
	evaluateObjective.addLinebreak( );

	evaluateObjective.addStatement( DyN.getTranspose() == objValueOut.getCols(0, NYN) );
	evaluateObjective.addLinebreak();

	if (QN1.isGiven() == false)
	{
		ExportVariable tmpQN1, tmpQN2;
		tmpQN1.setup("tmpQN1", NX, NX, REAL, ACADO_LOCAL);
		tmpQN2.setup("tmpQN2", NX, NYN, REAL, ACADO_LOCAL);

		setObjQN1QN2.setup("setObjQN1QN2", tmpFxEnd, tmpObjSEndTerm, tmpQN1, tmpQN2);
		setObjQN1QN2.addStatement( tmpQN2 == (tmpFxEnd ^ tmpObjSEndTerm) );
		setObjQN1QN2.addStatement( tmpQN1 == tmpQN2 * tmpFxEnd );
		setObjQN1QN2.addStatement( tmpQN1 += evLmX );

		indexX = getNYN();
		ExportArgument tmpFxEndCall = tmpFxEnd.isGiven() == true ? tmpFxEnd  : objValueOut.getAddress(0, indexX);

		evaluateObjective.addFunctionCall(
				setObjQN1QN2,
				tmpFxEndCall, objSEndTerm,
				QN1.getAddress(0, 0), QN2.getAddress(0, 0)
		);

		evaluateObjective.addLinebreak( );
	}
	else if (levenbergMarquardt > 0.0)
		QN1 = QN1.getGivenMatrix() + evLmX.getGivenMatrix();

	//
	// Hessian setup
	//

	ExportIndex index( "index" );

	//
	// Gradient setup
	//
	ExportVariable qq, rr;
	qq.setup("stageq", NX, 1, REAL, ACADO_LOCAL);
	rr.setup("stager", NU, 1, REAL, ACADO_LOCAL);


    ExportVariable SlxCall = objSlx.isGiven() == true ? objSlx : ExportVariable("Slx", NX, 1, REAL, ACADO_LOCAL);
    ExportVariable SluCall = objSlu.isGiven() == true ? objSlu : ExportVariable("Slu", NU, 1, REAL, ACADO_LOCAL);
	setStagef.setup("setStagef", qq, rr, SlxCall, SluCall, index);

	if (Q2.isGiven() == false)
		setStagef.addStatement(
				qq == Q2.getSubMatrix(index * NX, (index + 1) * NX, 0, NY) * Dy.getRows(index * NY, (index + 1) * NY)
		);
	else
	{
		setStagef << "(void)" << index.getFullName() << ";\n";
		setStagef.addStatement(
				qq == Q2 * Dy.getRows(index * NY, (index + 1) * NY)
		);
	}
    setStagef.addStatement( qq += SlxCall );
	setStagef.addLinebreak();

	if (R2.isGiven() == false)
		setStagef.addStatement(
				rr == R2.getSubMatrix(index * NU, (index + 1) * NU, 0, NY) * Dy.getRows(index * NY, (index + 1) * NY)
		);
	else
	{
		setStagef.addStatement(
				rr == R2 * Dy.getRows(index * NY, (index + 1) * NY)
		);
	}
	setStagef.addStatement( rr += SluCall );

	//
	// Setup necessary QP variables
	//

	if (Q1.isGiven() == true)
	{
		qpQ.setup("qpQ", N * NX, NX, REAL, ACADO_WORKSPACE);
		for (unsigned blk = 0; blk < N; ++blk)
			initialize.addStatement( qpQ.getSubMatrix(blk * NX, (blk + 1) * NX, 0, NX) == Q1);
	}
	else
	{
		qpQ = Q1;
	}

	if (R1.isGiven() == true)
	{
		qpR.setup("qpR", N * NU, NU, REAL, ACADO_WORKSPACE);
		for (unsigned blk = 0; blk < N; ++blk)
			initialize.addStatement( qpR.getSubMatrix(blk * NU, (blk + 1) * NU, 0, NU) == R1);
	}
	else
	{
		qpR = R1;
	}

	if (S1.isGiven() == true)
	{
		qpS.setup("qpS", N * NX, NU, REAL, ACADO_WORKSPACE);
		if (S1.getGivenMatrix().isZero() == true)
			initialize.addStatement(qpS == zeros<double>(N * NX, NU));
		else
			for (unsigned blk = 0; blk < N; ++blk)
				initialize.addStatement( qpS.getSubMatrix(blk * NX, (blk + 1) * NX, 0, NU) == S1);
	}
	else
	{
		qpS = S1;
	}

	if (QN1.isGiven() == true)
	{
		qpQf.setup("qpQf", NX, NX, REAL, ACADO_WORKSPACE);
		initialize.addStatement( qpQf == QN1 );
	}
	else
	{
		qpQf = QN1;
	}

	return SUCCESSFUL_RETURN;
}

returnValue ExportGaussNewtonGeneric::setupConstraintsEvaluation( void )
{
	////////////////////////////////////////////////////////////////////////////
	//
	// Setup evaluation of box constraints on states and controls
	//
	////////////////////////////////////////////////////////////////////////////



	int hardcodeConstraintValues;
	get(CG_HARDCODE_CONSTRAINT_VALUES, hardcodeConstraintValues);

	evaluateConstraints.setup("evaluateConstraints");

	DVector lbTmp, ubTmp;

	DVector lbXInf( NX );
	lbXInf.setAll( -INFTY );

	DVector ubXInf( NX );
	ubXInf.setAll( INFTY );

	DVector lbUInf( NU );
	lbUInf.setAll( -INFTY );

	DVector ubUInf( NU );
	ubUInf.setAll( INFTY );

	DVector lbValues, ubValues;

	//
	// Stack input bounds
	//
	for (unsigned node = 0; node < N; ++node)
	{
		lbTmp = uBounds.getLowerBounds( node );
		if ( !lbTmp.getDim() )
			lbValues.append( lbUInf );
		else
			lbValues.append( lbTmp );

		ubTmp = uBounds.getUpperBounds( node );
		if ( !ubTmp.getDim() )
			ubValues.append( ubUInf );
		else
			ubValues.append( ubTmp );
	}

	//
	// Stack state bounds
	//
	for (unsigned node = 1; node < N + 1; ++node)
	{
		lbTmp = xBounds.getLowerBounds( node );
		if ( !lbTmp.getDim() )
			lbValues.append( lbXInf );
		else
			lbValues.append( lbTmp );

		ubTmp = xBounds.getUpperBounds( node );
		if ( !ubTmp.getDim() )
			ubValues.append( ubXInf );
		else
			ubValues.append( ubTmp );
	}

	qpLb.setup("qpLb", N * NU + N * NX, 1, REAL, ACADO_WORKSPACE);
	qpUb.setup("qpUb", N * NU + N * NX, 1, REAL, ACADO_WORKSPACE);

	if( hardcodeConstraintValues == YES ) {
	    evLbValues.setup("evLbValues", lbValues, STATIC_CONST_REAL, ACADO_LOCAL);
	    evUbValues.setup("evUbValues", ubValues, STATIC_CONST_REAL, ACADO_LOCAL);

	    evaluateConstraints.addVariable( evLbValues );
	    evaluateConstraints.addVariable( evUbValues );
	}
	else {
	    evLbValues.setup("lbValues", N * NU + N * NX, 1, REAL, ACADO_VARIABLES);
	    evLbValues.setDoc( "Lower bounds values." );
	    evUbValues.setup("ubValues", N * NU + N * NX, 1, REAL, ACADO_VARIABLES);
	    evUbValues.setDoc( "Upper bounds values." );

	    initialize.addStatement( evLbValues == lbValues );
	    initialize.addStatement( evUbValues == ubValues );
	}

	evaluateConstraints.addStatement( qpLb.getRows(0, N * NU) == evLbValues.getRows(0, N * NU) - u.makeColVector() );
	evaluateConstraints.addStatement( qpUb.getRows(0, N * NU) == evUbValues.getRows(0, N * NU) - u.makeColVector() );

	evaluateConstraints.addStatement( qpLb.getRows(N * NU, N * NU + N * NX) == evLbValues.getRows(N * NU, N * NU + N * NX) - x.makeColVector().getRows(NX, NX * (N + 1)) );
	evaluateConstraints.addStatement( qpUb.getRows(N * NU, N * NU + N * NX) == evUbValues.getRows(N * NU, N * NU + N * NX) - x.makeColVector().getRows(NX, NX * (N + 1)) );



	////////////////////////////////////////////////////////////////////////////
	//
	// Setup evaluation of path and point constraints
	//
	////////////////////////////////////////////////////////////////////////////

	qpDimHtot  = N * dimPacH;
	qpDimH  = N * dimPacH;
	qpDimHN = 0;

	qpConDim.resize(N + 1, 0);
	for (unsigned i = 0; i < N; ++i)
		qpConDim[ i ] += dimPacH;

	for (unsigned i = 0; i < N; ++i)
		if (evaluatePointConstraints[ i ])
		{
			unsigned dim = evaluatePointConstraints[ i ]->getFunctionDim() / (1 + NX + NU);

			qpDimHtot  += dim;
			qpDimH  += dim;

			qpConDim[ i ] += dim;
		}

	if (evaluatePointConstraints[ N ])
	{
		unsigned dim = evaluatePointConstraints[ N ]->getFunctionDim() / (1 + NX);
		qpDimHtot  += dim;
		qpDimHN  += dim;

		qpConDim[ N ] += dim;
	}


	if (qpDimHtot) 	// this is a bit of a hack...
					// dummy qpUbA and qpLbA are created if there are no polytopic constraints
	{
		qpLbA.setup("qpLbA", qpDimHtot, 1, REAL, ACADO_WORKSPACE);
		qpUbA.setup("qpUbA", qpDimHtot, 1, REAL, ACADO_WORKSPACE);
		qpMu.setup("qpMu", 2 * N * (NX + NU) + 2*qpDimHtot, 1, REAL, ACADO_WORKSPACE);
	}
	else
	{
		qpLbA.setup("qpLbA", 1, 1, REAL, ACADO_WORKSPACE);
		qpUbA.setup("qpUbA", 1, 1, REAL, ACADO_WORKSPACE);
		qpMu.setup("qpMu", 2 * N * (NX + NU), 1, REAL, ACADO_WORKSPACE);
	}

	//
	// Setup constraint values for the whole horizon.
	//
	DVector lbAValues;
	DVector ubAValues;

	for (unsigned i = 0; i < N; ++i)
	{
		if ( dimPacH )
		{
			lbAValues.append( lbPathConValues.block(i * dimPacH, 0, dimPacH, 1) );
			ubAValues.append( ubPathConValues.block(i * dimPacH, 0, dimPacH, 1) );
		}
		lbAValues.append( pocLbStack[ i ] );
		ubAValues.append( pocUbStack[ i ] );
	}
	lbAValues.append( pocLbStack[ N ] );
	ubAValues.append( pocUbStack[ N ] );

	if( hardcodeConstraintValues == YES || qpDimHtot == 0 ) {
	    evLbAValues.setup("lbAValues", lbAValues, STATIC_CONST_REAL);
	    evUbAValues.setup("ubAValues", ubAValues, STATIC_CONST_REAL);

	    evaluateConstraints.addVariable( evLbAValues );
	    evaluateConstraints.addVariable( evUbAValues );
	}
	else {
	    evLbAValues.setup("lbAValues", qpDimHtot, 1, REAL, ACADO_VARIABLES);
	    evLbAValues.setDoc( "Lower affine bounds values." );
	    evUbAValues.setup("ubAValues", qpDimHtot, 1, REAL, ACADO_VARIABLES);
	    evUbAValues.setDoc( "Upper affine bounds values." );

        initialize.addStatement( evLbAValues == lbAValues );
        initialize.addStatement( evUbAValues == ubAValues );
	}

	//
	// Evaluate path constraints
	//

	if ( dimPacH )
	{
		ExportIndex runPac;
		evaluateConstraints.acquire( runPac );
		ExportForLoop loopPac(runPac, 0, N);

		loopPac.addStatement( conValueIn.getCols(0, NX) == x.getRow( runPac ) );
		loopPac.addStatement( conValueIn.getCols(NX, NX + NU) == u.getRow( runPac ) );
		loopPac.addStatement( conValueIn.getCols(NX + NU, NX + NU + NOD) == od.getRow( runPac ) );
		loopPac.addFunctionCall( evaluatePathConstraints.getName(), conValueIn, conValueOut );

		loopPac.addStatement( pacEvH.getRows( runPac * dimPacH, (runPac + 1) * dimPacH) ==
				conValueOut.getTranspose().getRows(0, dimPacH) );
		loopPac.addLinebreak( );

		unsigned derOffset = dimPacH;

		// Optionally store derivatives
		if (pacEvHx.isGiven() == false)
		{
		    for (unsigned j1 = 0; j1 < dimPacH; ++j1) {
		        for (unsigned j2 = 0; j2 < NX; ++j2) {
		            loopPac.addStatement(
		                    pacEvHx.getElement(runPac * dimPacH + j1, j2) ==
		                            conValueOut.getCol(derOffset + j1*NX+j2)
		            );
		        }
		    }

			derOffset = derOffset + dimPacH * NX;
		}
		if (pacEvHu.isGiven() == false )
		{
		    for (unsigned j1 = 0; j1 < dimPacH; ++j1) {
		        for (unsigned j2 = 0; j2 < NU; ++j2) {
		            loopPac.addStatement(
		                    pacEvHu.getElement(runPac * dimPacH + j1, j2) ==
		                            conValueOut.getCol(derOffset + j1*NU+j2)
		            );
		        }
		    }
		}

		// Add loop to the function.
		evaluateConstraints.addStatement( loopPac );
		evaluateConstraints.release( runPac );
		evaluateConstraints.addLinebreak( );
	}

	//
	// Evaluate point constraints
	//

	for (unsigned i = 0, intRowOffset = 0, dim = 0; i < N + 1; ++i)
	{
		if (evaluatePointConstraints[ i ] == 0)
			continue;

		evaluateConstraints.addComment(
				string( "Evaluating constraint on node: #" ) + toString( i )
		);

		evaluateConstraints.addStatement(conValueIn.getCols(0, getNX()) == x.getRow( i ) );
		if (i < N)
		{
			evaluateConstraints.addStatement( conValueIn.getCols(NX, NX + NU) == u.getRow( i ) );
			evaluateConstraints.addStatement( conValueIn.getCols(NX + NU, NX + NU + NOD) == od.getRow( i ) );
		}
		else
			evaluateConstraints.addStatement( conValueIn.getCols(NX, NX + NOD) == od.getRow( i ) );

		evaluateConstraints.addFunctionCall(
				evaluatePointConstraints[ i ]->getName(), conValueIn, conValueOut );
		evaluateConstraints.addLinebreak();

		if (i < N)
			dim = evaluatePointConstraints[ i ]->getFunctionDim() / (1 + NX + NU);
		else
			dim = evaluatePointConstraints[ i ]->getFunctionDim() / (1 + NX);

		// Fill pocEvH, pocEvHx, pocEvHu
		evaluateConstraints.addStatement(
				pocEvH.getRows(intRowOffset, intRowOffset + dim) ==
						conValueOut.getTranspose().getRows(0, dim));
		evaluateConstraints.addLinebreak();

		evaluateConstraints.addStatement(
				pocEvHx.makeRowVector().getCols(intRowOffset * NX, (intRowOffset + dim) * NX)
						== conValueOut.getCols(dim, dim + dim * NX));
		evaluateConstraints.addLinebreak();

		if (i < N)
		{
			evaluateConstraints.addStatement(
					pocEvHu.makeRowVector().getCols(intRowOffset * NU, (intRowOffset + dim) * NU)
							== conValueOut.getCols(dim + dim * NX, dim + dim * NX + dim * NU));
			evaluateConstraints.addLinebreak();
		}

		intRowOffset += dim;
	}

	//
	// Copy data to QP solver structures
	//

	ExportVariable tLbAValues, tUbAValues;
	ExportIndex offsetPac("offset"), indPac( "ind" );

	tLbAValues.setup("lbAValues", dimPacH, 1, REAL, ACADO_LOCAL);
	tUbAValues.setup("ubAValues", dimPacH, 1, REAL, ACADO_LOCAL);

	setStagePac.setup("setStagePac", offsetPac, indPac, tLbAValues, tUbAValues);

	setStagePac
		<< (qpLbA.getRows(offsetPac, offsetPac + dimPacH) == tLbAValues - pacEvH.getRows(indPac * dimPacH, indPac * dimPacH + dimPacH))
		<< (qpUbA.getRows(offsetPac, offsetPac + dimPacH) == tUbAValues - pacEvH.getRows(indPac * dimPacH, indPac * dimPacH + dimPacH));

	ExportVariable tPocA;
	tPocA.setup("tPocA", conValueOut.getDim(), NX + NU, REAL);
	if ( dimPocH )
		evaluateConstraints.addVariable( tPocA );

	unsigned offsetEval = 0;
	unsigned offsetPoc = 0;
	for (unsigned i = 0; i < N; ++i)
	{
		if ( dimPacH )
		{
			evaluateConstraints.addFunctionCall(
					setStagePac,
					ExportIndex( offsetEval ), ExportIndex( i ),
					evLbAValues.getAddress( offsetEval ), evUbAValues.getAddress( offsetEval )
			);

			offsetEval += dimPacH;
		}

		if ( evaluatePointConstraints[ i ] )
		{
			unsigned dim = evaluatePointConstraints[ i ]->getFunctionDim() / (1 + NX + NU);

			evaluateConstraints.addLinebreak();

			evaluateConstraints
				<< (tPocA.getSubMatrix(0, dim, 0, NX) == pocEvHx.getSubMatrix(offsetPoc, offsetPoc + dim, 0, NX))
				<< (tPocA.getSubMatrix(0, dim, NX, NX + NU) == pocEvHu.getSubMatrix(offsetPoc, offsetPoc + dim, 0, NU))
				<< (qpLbA.getRows(offsetEval, offsetEval + dim) ==
						evLbAValues.getRows(offsetEval, offsetEval + dim) - pocEvH.getRows(offsetPoc, offsetPoc + dim))
				<< (qpUbA.getRows(offsetEval, offsetEval + dim) ==
						evUbAValues.getRows(offsetEval, offsetEval + dim) - pocEvH.getRows(offsetPoc, offsetPoc + dim));

			offsetEval += dim;
			offsetPoc += dim;
		}
	}

	if ( evaluatePointConstraints[ N ] )
	{
		unsigned dim = evaluatePointConstraints[ N ]->getFunctionDim() / (1 + NX);

		evaluateConstraints
			<< (qpLbA.getRows(offsetEval, offsetEval + dim) ==
					evLbAValues.getRows(offsetEval, offsetEval + dim) - pocEvH.getRows(offsetPoc, offsetPoc + dim))
			<< (qpUbA.getRows(offsetEval, offsetEval + dim) ==
					evUbAValues.getRows(offsetEval, offsetEval + dim) - pocEvH.getRows(offsetPoc, offsetPoc + dim));
	}

	return SUCCESSFUL_RETURN;
}


returnValue ExportGaussNewtonGeneric::setupVariables( )
{
	if (initialStateFixed() == true)
	{
		x0.setup("x0",  NX, 1, REAL, ACADO_VARIABLES);
		x0.setDoc( "Current state feedback vector." );
	}
	else
	{
		xAC.setup("xAC", NX, 1, REAL, ACADO_VARIABLES);
		DxAC.setup("DxAC", NX, 1, REAL, ACADO_WORKSPACE);
		SAC.setup("SAC", NX, NX, REAL, ACADO_VARIABLES);
		sigmaN.setup("sigmaN", NX, NX, REAL, ACADO_VARIABLES);
	}

	return SUCCESSFUL_RETURN;
}

returnValue ExportGaussNewtonGeneric::setupMultiplicationRoutines( )
{
	return SUCCESSFUL_RETURN;
}

returnValue ExportGaussNewtonGeneric::setupEvaluation( )
{
    int gradientUp;
    get( LIFTED_GRADIENT_UPDATE, gradientUp );
    bool gradientUpdate = (bool) gradientUp;

    int variableObjS;
    get(CG_USE_VARIABLE_WEIGHTING_MATRIX, variableObjS);
    int sensitivityProp;
    get( DYNAMIC_SENSITIVITY, sensitivityProp );
    bool adjoint = ((ExportSensitivityType) sensitivityProp == BACKWARD || ((ExportSensitivityType) sensitivityProp == INEXACT && gradientUpdate));

	////////////////////////////////////////////////////////////////////////////
	//
	// Setup preparation phase
	//
	////////////////////////////////////////////////////////////////////////////
	preparation.setup("preparationStep");
	preparation.doc( "Preparation step of the RTI scheme." );

	ExportVariable retSim("ret", 1, 1, INT, ACADO_LOCAL, true);
	retSim.setDoc("Status of the integration module. =0: OK, otherwise the error code.");
	preparation.setReturnValue(retSim, false);

	preparation	<< retSim.getFullName() << " = " << modelSimulation.getName() << "();\n";

	preparation.addFunctionCall( evaluateObjective );
	preparation.addFunctionCall( evaluateConstraints );

	////////////////////////////////////////////////////////////////////////////
	//
	// Setup feedback phase
	//
	////////////////////////////////////////////////////////////////////////////
	ExportVariable stateFeedback("stateFeedback", NX, 1, REAL, ACADO_LOCAL);
	ExportVariable returnValueFeedbackPhase("retVal", 1, 1, INT, ACADO_LOCAL, true);
	returnValueFeedbackPhase.setDoc( "Status code of the QP solver." );
	feedback.setup("feedbackStep" );
	feedback.doc( "Feedback/estimation step of the RTI scheme." );
	feedback.setReturnValue( returnValueFeedbackPhase );

	qpx.setup("qpx", NX * (N + 1), 1, REAL, ACADO_WORKSPACE);
	qpu.setup("qpu", NU * N,       1, REAL, ACADO_WORKSPACE);

	qpq.setup("qpq", NX * N, 1, REAL, ACADO_WORKSPACE);
	qpqf.setup("qpqf", NX, 1, REAL, ACADO_WORKSPACE);
	qpr.setup("qpr", NU * N, 1, REAL, ACADO_WORKSPACE);

	qpLambda.setup("qpLambda", N * NX, 1, REAL, ACADO_WORKSPACE);

	nIt.setup("nIt", 1, 1, INT, ACADO_WORKSPACE);


	if (initialStateFixed() == false)
	{
	}
	else
	{
		// State feedback
		feedback.addStatement( qpx.getRows(0, NX) == x0 - x.getRow( 0 ).getTranspose() );
	}

	//
	// Calculate objective residuals
	//
	feedback.addStatement( Dy -= y );
	feedback.addLinebreak();
	feedback.addStatement( DyN -= yN );
	feedback.addLinebreak();

	for (unsigned i = 0; i < N; ++i) {
        ExportArgument SlxCall =
                objSlx.isGiven() == true || (variableObjS == false && !adjoint) ? objSlx : objSlx.getAddress(i * NX, 0);
        ExportArgument SluCall =
                objSlu.isGiven() == true || (variableObjS == false && !adjoint) ? objSlu : objSlu.getAddress(i * NU, 0);

		feedback.addFunctionCall(setStagef, qpq.getAddress(i * NX), qpr.getAddress(i * NU), SlxCall, SluCall, ExportIndex( i ));
	}
	feedback.addLinebreak();
    ExportVariable SlxCall =
                objSlx.isGiven() == true || (variableObjS == false && !adjoint) ? objSlx : objSlx.getRows(N * NX, (N + 1) * NX);
	feedback.addStatement( qpqf == QN2 * DyN );
    feedback.addStatement( qpqf += SlxCall );

	feedback.addLinebreak();

//	//
//	// Arrival cost in the MHE case
//	//
//	if (initialStateFixed() == false)
//	{
//		// It is assumed this is the shifted version from the previous time step!
//		feedback.addStatement( DxAC == xAC - x.getRow( 0 ).getTranspose() );
//	}

	if (SAC.getDim() > 0)
	{
	    // Include arrival cost
	    feedback.addStatement( DxAC == x.getRow( 0 ).getTranspose() - xAC );
        feedback.addStatement( Q1.getRows(0,NX) += SAC );
	    feedback.addStatement( qpq.getRows(0,NX) += SAC*DxAC );
	}

	//
	// Here we have to add the differences....
	//

	string moduleName;
	get(CG_MODULE_NAME, moduleName);

	// Call the solver
	feedback << returnValueFeedbackPhase.getFullName() << " = " << moduleName << "_solve( );\n";

	// Accumulate the solution, i.e. perform full Newton step
	feedback.addStatement( x.makeColVector() += qpx );
	feedback.addStatement( u.makeColVector() += qpu );

//	if (initialStateFixed() == false)
//	{
//		// This is the arrival cost for the next time step!
//		feedback.addStatement( xAC == x.getRow( 1 ).getTranspose() + DxAC );
//	}

	////////////////////////////////////////////////////////////////////////////
	//
	// Setup evaluation of the KKT tolerance
	//
	////////////////////////////////////////////////////////////////////////////

	ExportVariable kkt("kkt", 1, 1, REAL, ACADO_LOCAL, true);
	ExportVariable tmp("tmp", 1, 1, REAL, ACADO_LOCAL, true);
	ExportIndex index( "index" );

	getKKT.setup( "getKKT" );
	getKKT.doc( "Get the KKT tolerance of the current iterate." );
	kkt.setDoc( "The KKT tolerance value." );
	getKKT.setReturnValue( kkt );
	getKKT.addVariable( tmp );
	getKKT.addIndex( index );

	getKKT.addStatement( kkt == 0.0 );

	// XXX This is still probably relevant for the NMPC case

	getKKT.addStatement( tmp == (qpq ^ qpx.getRows(0, N * NX)) );
	getKKT << kkt.getFullName() << " += fabs( " << tmp.getFullName() << " );\n";
	getKKT.addStatement( tmp == (qpqf ^ qpx.getRows(N * NX, (N + 1) * NX)) );
	getKKT << kkt.getFullName() << " += fabs( " << tmp.getFullName() << " );\n";
	getKKT.addStatement( tmp == (qpr ^ qpu) );
	getKKT << kkt.getFullName() << " += fabs( " << tmp.getFullName() << " );\n";

	ExportForLoop lamLoop(index, 0, N * NX);
	lamLoop << kkt.getFullName() << "+= fabs( " << d.get(index, 0) << " * " << qpLambda.get(index, 0) << ");\n";
	getKKT.addStatement( lamLoop );

	if (initialStateFixed() == true)
	{
		// XXX This is because the MHE does not support inequality constraints at the moment

		ExportForLoop lbLoop(index, 0, N * NU + N * NX);
		lbLoop << kkt.getFullName() << "+= fabs( " << qpLb.get(index, 0) << " * " << qpMu.get(index, 0) << ");\n";
		ExportForLoop ubLoop(index, 0, N * NU + N * NX);
		ubLoop << kkt.getFullName() << "+= fabs( " << qpUb.get(index, 0) << " * " << qpMu.get(index + N * NU + N * NX, 0) << ");\n";
		ExportForLoop lgLoop(index, 0, qpDimHtot);
		lgLoop << kkt.getFullName() << "+= fabs( " << qpLbA.get(index, 0) << " * " << qpMu.get(index + 2*N*(NU+NX), 0) << ");\n";
		ExportForLoop ugLoop(index, 0, qpDimHtot);
		ugLoop << kkt.getFullName() << "+= fabs( " << qpUbA.get(index, 0) << " * " << qpMu.get(index + 2*N*(NU+NX) + qpDimHtot, 0) << ");\n";

		getKKT.addStatement( lbLoop );
		getKKT.addStatement( ubLoop );
		getKKT.addStatement( lgLoop );
		getKKT.addStatement( ugLoop );
	}

	return SUCCESSFUL_RETURN;
}

CLOSE_NAMESPACE_ACADO
