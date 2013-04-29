/*-------------------------------------------------------------------
Copyright 2013 Deniz Gunceler

This file is part of JDFTx.

JDFTx is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

JDFTx is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with JDFTx.  If not, see <http://www.gnu.org/licenses/>.
-------------------------------------------------------------------*/

#ifndef JDFTX_ELECTRONIC_RESIDUALMINIMIZERPARAMS_H
#define JDFTX_ELECTRONIC_RESIDUALMINIMIZERPARAMS_H

enum MixedVariable
{
	density, //! Mix electron density (n) and kinetic energy density (tau)
	potential //! Mix the local electronic potential (Vscloc) and the kinetic energy potential (Vtau)
};

struct ResidualMinimizerParams
{
	int nIterations; //! maximum iterations (single point calculation if 0)
	double energyDiffThreshold; //! convergence threshold for energy difference between successive iterations
	MixedVariable mixedVariable; //! Whether we are mixing the density or the potential
	
	ResidualMinimizerParams()
	{
		nIterations = 10;
		energyDiffThreshold = 1e-6;
		mixedVariable = density;
	}
	
};

#endif // JDFTX_ELECTRONIC_RESIDUALMINIMIZERPARAMS_H