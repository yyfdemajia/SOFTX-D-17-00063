/*-------------------------------------------------------------------
Copyright 2012 Ravishankar Sundararaman

This file is part of Fluid1D.

Fluid1D is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fluid1D is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fluid1D.  If not, see <http://www.gnu.org/licenses/>.
-------------------------------------------------------------------*/

#ifndef FLUID1D_FLUID1D_MOLECULE_H
#define FLUID1D_FLUID1D_MOLECULE_H

#include <core/Data.h>
#include <core/vector3.h>
#include <core/string.h>
#include <vector>
#include <map>


//! Properties of a site in a multi-site molecule model
struct SiteProperties
{
	const double sphereRadius; //!< Hard sphere radius for this site in mixed FMT (set 0 to disable)
	const double sphereSigma; //!< Erf width to soften sphere in mixed FMT (set 0 for hard sphere)

	const double chargeZ; //!< site charge within the classical DFT
	const SphericalKernel* chargeKernel; //!< Charge profile within classical DFT (reformulation of high frequency cutoff of coulomb kernel)
	const bool indepSite; //!< Whether this site contributes to the independent variable list

	const double alpha; //!< site polarizability
	const SphericalKernel* alphaKernel; //!< polarizability kernel (high-frequency cutoff)
	
	//! Initailize all the const members above, and create kernels for FMT if sphereRadius is non-zero
	SiteProperties(const GridInfo& gInfo, double sphereRadius, double sphereSigma,
		double chargeZ, const SphericalKernel* chargeKernel, bool indepSite=true,
		double alpha=0., const SphericalKernel* alphaKernel=0);
	~SiteProperties();

private:
	SphericalKernel *w0, *w1, *w2, *w3, *w1v, *w2m; //!< Soft sphere weight functions
	friend class FluidMixture;
};


//! A single site in a multi-site molecule model
//! If the molecule has n-fold rotation symmetry about some axis,
//! then pick that to be the z-axis and use SO3/Zn sampling.
//! The dipole moment, if any, MUST be along the z-axis.
struct Site
{	int index; //!< Site density index: sites related by symmetry in a molecule will share the same value
	SiteProperties* prop; //!< Site properties: multiple symmetry classes and different same species in different molecules could share the same
	vector3<> pos; //!< Position w.r.t molecular origin in the reference orientation
};

//! Molecule: a collection of sites
struct Molecule
{	const string name; //!< An identifier for molecule (used for EnergyComponent labels)
	const std::vector<Site> site; //!< list of sites
	const int nSites; //!< total number of sites (including multiplicities) equal to site.size()
	const int nIndices; //!< number of distinguishable sites after symmetry, which is the same as number of site densities/psi's required

	/** Template-Magic constructor, for example to construct the bonded-void geometry
	which consists of O, 2x H and 2x V with Z2 symmetry:

	Molecule(&siteO,posO, &siteH,posH1,posH2, &siteV,posV1,posV2)

	The SiteProperties* arguments delimit the symmetry equivalence classes,
	and the site density indices are automatically assigned.
	*/
	template<typename... Args> Molecule(string name, Args... args)
	: name(name), site(make_site(args...)), nSites(site.size()), nIndices(site.back().index+1)
	{
	}

	double get_charge() const; //!< return the total charge (sum of chargeZ*chargeKernel->data[0] over all the sites)

	double get_dipole() const; //!< electric dipole moment (will be along +/- z by assumed Z2 symmetry)

	//! Get the harmonic sum of radii for spheres in contact, with the multiplicities for each such pair
	std::map<double,int> getBonds() const;

	std::vector<double> getLJweights() const; //!< Get the relative contributions for the scalar EOS term on each index (based on alpha)
	
private:
	//Template recursion entry point for the magic constructor above:
	template<typename... Args> std::vector<Site> make_site(SiteProperties* prop, vector3<> pos, Args... args)
	{	std::vector<Site> site;
		site.push_back(Site({0,prop,pos}));
		add_site(site, args...);
		return site;
	}
	//Initialize a new symmetry equivalence class in the recursion:
	template<typename... Args> void add_site(std::vector<Site>& site, SiteProperties* prop, vector3<> pos, Args... args)
	{	int index = site.back().index+1;
		site.push_back(Site({index,prop,pos}));
		add_site(site, args...);
	}
	//Continue previous symmetry equivalence class in the recursion:
	template<typename... Args> void add_site(std::vector<Site>& site, vector3<> pos, Args... args)
	{	int index = site.back().index;
		SiteProperties* prop = site.back().prop;
		site.push_back(Site({index,prop,pos}));
		add_site(site, args...);
	}
	//End recursion (all the arguments have been processed):
	void add_site(std::vector<Site>& site) {}

};

#endif // FLUID1D_FLUID1D_MOLECULE_H
