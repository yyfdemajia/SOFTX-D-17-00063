/*-------------------------------------------------------------------
Copyright 2014 Ravishankar Sundararaman

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

#include <wannier/WannierMinimizer.h>
#include <electronic/operators.h>

void WannierMinimizer::saveMLWF()
{	for(int iSpin=0; iSpin<nSpins; iSpin++)
		saveMLWF(iSpin);
}

void WannierMinimizer::saveMLWF(int iSpin)
{	
	//Compute the overlap matrices and initial rotations for current group of centers:
	for(int jProcess=0; jProcess<mpiUtil->nProcesses(); jProcess++)
	{	//Send/recv wavefunctions to other processes:
		Cother.assign(e.eInfo.nStates, ColumnBundle());
		if(jProcess == mpiUtil->iProcess()) //send
		{	for(int q=e.eInfo.qStart; q<e.eInfo.qStop; q++)
				((ColumnBundle&)e.eVars.C[q]).bcast(jProcess);
		}
		else //recv
		{	for(int q=e.eInfo.qStartOther(jProcess); q<e.eInfo.qStopOther(jProcess); q++)
			{	Cother[q].init(nBands, e.basis[q].nbasis, &e.basis[q], &e.eInfo.qnums[q]);
				Cother[q].bcast(jProcess);
			}
		}
		
		for(size_t ik=0; ik<kMesh.size(); ik++) if(isMine_q(ik,iSpin))
		{	KmeshEntry& ke = kMesh[ik];
			ColumnBundle Ci = getWfns(ke.point, iSpin); //Bloch functions at ik
			//Overlap with neighbours:
			for(EdgeFD& edge: ke.edge)
				if(whose_q(edge.ik,iSpin)==jProcess)
				{	//Pick up result from reverse edge if it has already been computed:
					bool foundReverse = false;
					if(jProcess==mpiUtil->iProcess() && edge.ik<ik)
					{	auto neighbourEdges = kMesh[edge.ik].edge;
						for(const EdgeFD& reverseEdge: neighbourEdges)
							if(reverseEdge.ik==ik)
							{	edge.M0 = dagger(reverseEdge.M0);
								foundReverse = true;
								break;
							}
					}
					//Compute overlap if reverse edge not yet computed:
					if(!foundReverse)
						edge.M0 = overlap(Ci, getWfns(edge.point, iSpin));
				}
			if(!jProcess) //Do only once (will get here multiple times for local wfns)
			{	//Band ranges:
				int bStart=0, bStop=0, bFixedStart=0, bFixedStop=0;
				if(wannier.outerWindow)
				{	const std::vector<double>& eigs = e.eVars.Hsub_eigs[ke.point.iReduced + iSpin*qCount];
					bStart = 0;
					while(bStart<nBands && eigs[bStart]<wannier.eOuterMin)
						bStart++;
					bStop = bStart;
					while(bStop<nBands && eigs[bStop]<=wannier.eOuterMax)
						bStop++;
					if(bStop-bStart < nCenters)
						die("Number of bands within outer window = %d less than nCenters = %d at k = [ %lg %lg %lg ]\n",
							bStop-bStart, nCenters, ke.point.k[0], ke.point.k[1], ke.point.k[2]);
					//Optionally range for inner window:
					if(wannier.innerWindow)
					{	bFixedStart = bStart;
						while(bFixedStart<bStop && eigs[bFixedStart]<wannier.eInnerMin)
							bFixedStart++;
						bFixedStop = bFixedStart;
						while(bFixedStop<bStop && eigs[bFixedStop]<=wannier.eInnerMax)
							bFixedStop++;
						if(bFixedStop-bFixedStart > nCenters)
							die("Number of bands within inner window = %d exceeds nCenters = %d at k = [ %lg %lg %lg ]\n",
								bFixedStop-bFixedStart, nCenters, ke.point.k[0], ke.point.k[1], ke.point.k[2]);
					}
					else bFixedStart = bFixedStop = bStart; //fixed interval is empty
				}
				else //fixed bands
				{	bFixedStart = bStart = wannier.bStart;
					bFixedStop  = bStop  = wannier.bStart + nCenters;
				}
				
				//Initial rotation of bands to get to Wannier subspace:
				matrix CdagG = Ci ^ trialWfns(ke.point);
				ke.nFixed = bFixedStop - bFixedStart;
				int nFree = nCenters - ke.nFixed;
				ke.nIn = (nFree>0) ? (bStop-bStart) : nCenters; //number of bands contributing to Wannier subspace
				ke.U1 = zeroes(nBands, ke.nIn);
				//--- Pick up fixed bands directly
				if(ke.nFixed > 0)
					ke.U1.set(bFixedStart,bFixedStop, 0,ke.nFixed, eye(ke.nFixed));
				//--- Pick up best linear combination of remaining bands (if any)
				if(nFree > 0)
				{	//Create overlap matrix with contribution from fixed bands projected out:
					matrix CdagGFree;
					if(ke.nFixed > 0)
					{	//SVD the fixed band contribution to the trial space:
						matrix U, Vdag; diagMatrix S;
						CdagG(bFixedStart,bFixedStop, 0,nCenters).svd(U, S, Vdag);
						//Project out the fixed bands (use only the zero singular values)
						CdagGFree = CdagG * dagger(Vdag(ke.nFixed,nCenters, 0,nCenters));
					}
					else CdagGFree = CdagG;
					//Truncate to non-zero rows:
					int nLo = bFixedStart-bStart;
					int nHi = bStop-bFixedStop;
					int nOuter = nLo+nHi;
					matrix CdagGFreeNZ = zeroes(nOuter, nOuter);
					if(nLo>0) CdagGFreeNZ.set(0,nLo, 0,nFree, CdagGFree(bStart,bFixedStart, 0,nFree));
					if(nHi>0) CdagGFreeNZ.set(nLo,nOuter, 0,nFree, CdagGFree(bFixedStop,bStop, 0,nFree));
					//SVD to get best linear combinations first:
					matrix U, Vdag; diagMatrix S;
					CdagGFreeNZ.svd(U, S, Vdag);
					//Convert left space from non-zero to all bands:
					matrix Upad = zeroes(nBands, nOuter);
					if(nLo>0) Upad.set(bStart,bFixedStart, 0,nOuter, U(0,nLo, 0,nOuter));
					if(nHi>0) Upad.set(bFixedStop,bStop, 0,nOuter, U(nLo,nOuter, 0,nOuter));
					//Include this combination in U1:
					ke.U1.set(0,nBands, ke.nFixed,ke.nIn, Upad * Vdag);
				}
				
				//Optimal initial rotation within Wannier subspace:
				matrix WdagG = dagger(ke.U1(0,nBands, 0,nCenters)) * CdagG;
				ke.U2 = WdagG * invsqrt(dagger(WdagG) * WdagG);
			}
		}
	}
	suspendOperatorThreading();
	
	//Broadcast overlaps and initial rotations:
	for(size_t ik=0; ik<kMesh.size(); ik++)
	{	KmeshEntry& ke = kMesh[ik];
		for(EdgeFD& edge: ke.edge)
		{	if(!isMine_q(ik,iSpin)) edge.M0 = zeroes(nBands, nBands);
			edge.M0.bcast(whose_q(ik,iSpin));
			if(!isMine(ik)) edge.M0 = matrix(); //not needed any more on this process
		}
		mpiUtil->bcast(ke.nIn, whose_q(ik,iSpin));
		mpiUtil->bcast(ke.nFixed, whose_q(ik,iSpin));
		if(!isMine_q(ik,iSpin))
		{	ke.U1 = zeroes(nBands, ke.nIn);
			ke.U2 = zeroes(nCenters, nCenters);
		}
		ke.U1.bcast(whose_q(ik,iSpin));
		ke.U2.bcast(whose_q(ik,iSpin));
		ke.B = zeroes(nCenters, ke.nIn);
	}
	
	//Minimize:
	double Omega = minimize(wannier.minParams);
	logPrintf("\nOptimum spread:\n\tOmega:  %.15le\n\tOmegaI: %.15le\n", Omega, OmegaI);
	
	//List the centers:
	logPrintf("\nCenters in %s coords:\n", e.iInfo.coordsType==CoordsCartesian ? "cartesian" : "lattice");
	for(int n=0; n<nCenters; n++)
	{	vector3<> rCoords = e.iInfo.coordsType==CoordsCartesian
			? rExpect[n] : e.gInfo.invR * rExpect[n]; //r in coordinate system of choice
		logPrintf("\t[ %lg %lg %lg ] spread: %lg bohr^2\n", rCoords[0], rCoords[1], rCoords[2], rSqExpect[n] - rExpect[n].length_squared());
	}
	logFlush();
	
	//Save the matrices:
	string fname = wannier.getFilename(false, "mlwfU", &iSpin);
	logPrintf("Dumping '%s' ... ", fname.c_str());
	if(mpiUtil->isHead())
	{	FILE* fp = fopen(fname.c_str(), "w");
		for(const auto& kMeshEntry: kMesh)
			kMeshEntry.U.write(fp);
		fclose(fp);
	}
	logPrintf("done.\n"); logFlush();

	if(wannier.saveWfns)
	{	resumeOperatorThreading();
		//--- Compute supercell wavefunctions:
		logPrintf("Computing supercell wavefunctions ... "); logFlush();
		ColumnBundle Csuper(nCenters, basisSuper.nbasis, &basisSuper, &qnumSuper, isGpuEnabled());
		Csuper.zero();
		for(unsigned i=0; i<kMesh.size(); i++) if(isMine_q(i,iSpin))
			Csuper += getWfns(kMesh[i].point, iSpin, true) * (kMesh[i].U * kMesh[i].point.weight);
		Csuper.allReduce(MPIUtil::ReduceSum);
		Csuper = translate(Csuper, vector3<>(.5,.5,.5)); //center in supercell
		logPrintf("done.\n"); logFlush();
		
		//--- Save supercell wavefunctions:
		for(int n=0; n<nCenters; n++)
		{	//Generate filename
			ostringstream varName;
			varName << n << ".mlwf";
			string fname = wannier.getFilename(false, varName.str(), &iSpin);
			logPrintf("Dumping '%s':\n", fname.c_str());
			//Convert to real space and remove phase:
			complexDataRptr psi = I(Csuper.getColumn(n));
			if(qnumSuper.k.length_squared() > symmThresholdSq)
				multiplyBlochPhase(psi, qnumSuper.k);
			complex* psiData = psi->data();
			double meanPhase, sigmaPhase, rmsImagErr;
			removePhase(gInfoSuper.nr, psiData, meanPhase, sigmaPhase, rmsImagErr);
			logPrintf("\tPhase = %lf +/- %lf\n", meanPhase, sigmaPhase); logFlush();
			logPrintf("\tRMS imaginary part = %le (after phase removal)\n", rmsImagErr);
			logFlush();
			//Write real part of supercell wavefunction to file:
			if(mpiUtil->isHead())
			{	FILE* fp = fopen(fname.c_str(), "wb");
				if(!fp) die("Failed to open file '%s' for binary write.\n", fname.c_str());
				for(int i=0; i<gInfoSuper.nr; i++)
					fwrite(psiData+i, sizeof(double), 1, fp);
				fclose(fp);
			}
		}
		suspendOperatorThreading();
	}
	
	//Save Hamiltonian in Wannier basis:
	std::vector<matrix> Hwannier(iCellMap.size());
	for(unsigned i=0; i<kMesh.size(); i++) if(isMine_q(i,iSpin))
	{	//Fetch Hamiltonian for subset of bands in center:
		matrix Hsub = e.eVars.Hsub_eigs[kMesh[i].point.iReduced + iSpin*qCount];
		//Apply MLWF-optimized rotation:
		Hsub = dagger(kMesh[i].U) * Hsub * kMesh[i].U;
		//Accumulate with each requested Bloch phase
		vector3<int> iSuper;
		std::vector<matrix>::iterator HwannierIter = Hwannier.begin();
		for(auto cell: iCellMap)
			*(HwannierIter++) += (cell.second * kMesh[i].point.weight * cis(2*M_PI*dot(kMesh[i].point.k, cell.first))) * Hsub;
	}
	for(matrix& H: Hwannier) H.allReduce(MPIUtil::ReduceSum);
	//-- save to file
	fname = wannier.getFilename(false, "mlwfH", &iSpin);
	logPrintf("Dumping '%s' ... ", fname.c_str()); logFlush();
	if(mpiUtil->isHead())
	{	FILE* fp = fopen(fname.c_str(), "wb");
		if(!fp) die("Failed to open file '%s' for binary write.\n", fname.c_str());
		for(matrix& H: Hwannier) H.write_real(fp);
		fclose(fp);
	}
	logPrintf("done.\n"); logFlush();
	resumeOperatorThreading();
}