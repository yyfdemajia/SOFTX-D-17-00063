/*-------------------------------------------------------------------
Copyright 2011 Ravishankar Sundararaman

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

#include <commands/command.h>
#include <electronic/Everything.h>


enum DebugOptions
{	DebugEigsFillings,
	DebugEcomponents,
	DebugMuSearch,
	DebugForces,
	DebugSymmetries,
	DebugFluid,
	DebugWannier,
	DebugDelim //delimiter to figure out end of input
};

EnumStringMap<DebugOptions> debugMap
(	DebugEigsFillings, "EigsFillings",
	DebugEcomponents, "Ecomponents",
	DebugMuSearch, "MuSearch",
	DebugForces, "Forces",
	DebugSymmetries, "Symmetries",
	DebugFluid, "Fluid",
	DebugWannier, "Wannier"
);

EnumStringMap<DebugOptions> debugDescMap
(	DebugEigsFillings, "Print eigenvalues, Hsub and fillings after each iteration",
	DebugEcomponents, "Print energy components and after each electronic iteration",
	DebugMuSearch, "Print progress of the mu bisect/fit routines",
	DebugForces, "Print each contribution to the force separately (NL, loc etc.)",
	DebugSymmetries, "Print various symmetry matrices during start up",
	DebugFluid, "Enable verbose logging of fluid (iterations for Linear, even more for others)",
	DebugWannier, "Log minimization progress for Wannier functions"
);

struct CommandDebug : public Command
{
	CommandDebug() : Command("debug")
	{
		format = "<option> <option> ...";
		comments =
			"Each <option> is one of "
			+ addDescriptions(debugMap.optionList(), linkDescription(debugMap, debugDescMap))
			+ "\nDefault: none of the optional outputs";
		allowMultiple = true;
		hasDefault = false;
	}

	void process(ParamList& pl, Everything& e)
	{	while(true)
		{	DebugOptions option;
			pl.get(option, DebugDelim, debugMap, "option");
			switch(option)
			{	case DebugEigsFillings:
					e.cntrl.shouldPrintEigsFillings = true;
					break;
				case DebugEcomponents:
					e.cntrl.shouldPrintEcomponents = true;
					break;
				case DebugMuSearch:
					e.cntrl.shouldPrintMuSearch = true;
					break;
				case DebugForces:
					e.iInfo.shouldPrintForceComponents = true;
					break;
				case DebugSymmetries:
					e.symm.shouldPrintMatrices = true;
					break;
				case DebugFluid:
					e.eVars.fluidParams.verboseLog = true;
					break;
				case DebugWannier:
					e.dump.wannier.verboseLog = true;
					break;
				case DebugDelim:
					return; //end of input line
			}
		}
	}

	void printStatus(Everything& e, int iRep)
	{	//Coalesce all the debug commands into a single one:
		if(iRep==0)
		{	if(e.cntrl.shouldPrintEigsFillings) logPrintf(" EigsFillings");
			if(e.cntrl.shouldPrintEcomponents) logPrintf(" Ecomponents");
			if(e.cntrl.shouldPrintMuSearch) logPrintf(" MuSearch");
			if(e.iInfo.shouldPrintForceComponents) logPrintf(" Forces");
			if(e.symm.shouldPrintMatrices) logPrintf("Symmetries");
			if(e.eVars.fluidParams.verboseLog) logPrintf(" Fluid");
			if(e.dump.wannier.verboseLog) logPrintf(" Wannier");
		}
	}
}
commandDebug;