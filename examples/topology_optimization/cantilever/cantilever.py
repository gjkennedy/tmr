"""
Cantilever example using mass-constrained compliance minimization.

This example demonstrates:

1) Creating meshes using the TMR.Creator classes
2) Lagrange-type filters
3) Design-feature based adaptive refinement

Recommended arguments:

mpirun -np n python cantilever.py

This code performs a minimum compliance optimization with a fixed mass
constraint. The design domain is a prismatic beam with a square cross-section.
The domain has an aspect ratio of 5. After one cycle of optimization, the
domain is refined, refining elements where there is material, and coarsening
where there is void.
"""

from mpi4py import MPI
from tmr import TMR, TopOptUtils
from paropt import ParOpt
from tacs import TACS, elements, constitutive, functions
import numpy as np
import argparse
import os

class OctCreator(TMR.OctTopoCreator):
    """
    An instance of an OctCreator class.

    This creates discretization for a Largange type filter, where the density is
    interpolated from the nodes of a coarser finite-element mesh. In this type of
    creator, the filter element mesh and the octree element mesh need be the same.
    (In a conformal filter, they must have the same element mesh but may have
    different degree of approximation.)
    """
    def __init__(self, bcs, filt, props):
        TMR.OctTopoCreator.__init__(bcs, filt)
        self.props = props

    def createElement(self, order, octant, index, weights):
        """
        Create the element for the given octant.

        This callback provides the global indices for the filter mesh and the weights
        applied to each nodal density value to obtain the element density. The
        local octant is also provided (but not used here).

        Args:
            order (int): Order of the underlying mesh
            octant (Octant): The TMR.Octant class
            index (list): List of the global node numbers referenced by the element
            weights (list): List of weights to compute the element density

        Returns:
            TACS.Element: Element for the given octant
        """
        stiff = TMR.OctStiffness(self.props, index, weights)
        elem = elements.Solid(2, stiff)
        return elem

class CreatorCallback:
    def __init__(self, bcs, props):
        self.bcs = bcs
        self.props = props

    def creator_callback(self, forest):
        """
        Create the creator class and filter for the provided OctForest object.

        This is called for every mesh level when the topology optimization
        problem is created.

        Args:
            forest (OctForest): The OctForest for this mesh level

        Returns:
            OctTopoCreator, OctForest: The creator and filter for this forest
        """
        filtr = forest.duplicate()
        filtr.coarsen()
        creator = OctCreator(self.bcs, filtr, self.props)
        return creator, filtr

def create_forest(comm, depth):
    """
    Create an initial forest for analysis. and optimization

    This code loads in the model, sets names, meshes the geometry and creates
    a QuadForest from the mesh. The forest is populated with quadtrees with
    the specified depth.

    Args:
        comm (MPI_Comm): MPI communicator
        depth (int): Depth of the initial trees
        htarget (float): Target global element mesh size

    Returns:
        OctForest: Initial forest for topology optimization
    """
    # Load the geometry model
    geo = TMR.LoadModel('cantilever.stp')

    # Mark the boundary condition faces
    verts = geo.getVertices()
    faces = geo.getFaces()
    volumes = geo.getVolumes()

    # Set source and target faces
    faces[3].setName('fixed')
    faces[4].setSource(volumes[0], faces[5])
    verts[4].setName('pt1')
    verts[3].setName('pt2')

    # Create the mesh
    mesh = TMR.Mesh(comm, geo)

    # Set the meshing options
    opts = TMR.MeshOptions()

    # Create the surface mesh
    htarget = 5.0
    mesh.mesh(htarget, opts)

    # Create a model from the mesh
    model = mesh.createModelFromMesh()

    # Create the corresponding mesh topology from the mesh-model
    topo = TMR.Topology(comm, model)

    # Create the quad forest and set the topology of the forest
    forest = TMR.OctForest(comm)
    forest.setTopology(topo)

    # Create the trees, rebalance the elements and repartition
    forest.createTrees(depth)

    return forest

def create_problem(forest, bcs, props, nlevels):
    """
    Create the TMRTopoProblem object and set up the topology optimization problem.

    This code is given the forest, boundary conditions, material properties and
    the number of multigrid levels. Based on this info, it creates the TMRTopoProblem
    and sets up the mass-constrained compliance minimization problem. Before
    the problem class is returned it is initialized so that it can be used for
    optimization.

    Args:
        forest (OctForest): Forest object
        bcs (BoundaryConditions): Boundary condition object
        props (StiffnessProperties): Material properties object
        nlevels (int): number of multigrid levels

    Returns:
        TopoProblem: Topology optimization problem instance
    """

    # Create the problem and filter object
    obj = CreatorCallback(bcs, props)
    problem = TopOptUtils.createTopoProblem(forest,
        obj.creator_callback, filter_type, nlevels=nlevels)

    # Get the assembler object we just created
    assembler = problem.getAssembler()

    # Set the load
    P = 1.0e3
    force = TopOptUtils.computeVertexLoad('pt1', forest, assembler, [0, P, 0])
    temp = TopOptUtils.computeVertexLoad('pt2', forest, assembler, [0, 0, P])
    force.axpy(1.0, temp)

    # Set the load cases into the topology optimization problem
    problem.setLoadCases([force])

    # Compute the fixed mass target
    lx = 50.0 # mm
    ly = 10.0 # mm
    lz = 10.0 # mm
    vol = lx*ly*lz
    vol_frac = 0.25
    density = 2600.0
    m_fixed = vol_frac*(vol*density)

    # Set the mass constraint
    funcs = [functions.StructuralMass(assembler)]
    problem.addConstraints(0, funcs, [-m_fixed], [-1.0/m_fixed])

    # Set the objective (scale the compliance objective)
    problem.setObjective([1.0e3])

    # Initialize the problem and set the prefix
    problem.initialize()

    return problem

# Set the optimization parameters
optimization_options = {
    # Parameters for the trust region method
    'tr_init_size': 0.01,
    'tr_max_size': 0.1,
    'tr_min_size': 0.01,
    'tr_eta': 0.25,
    'tr_penalty_gamma': 20.0,

    # Parameters for the interior point method (used to solve the
    # trust region subproblem)
    'max_qn_subspace': 7,
    'qn_diag_factor': 0.01,
    'bfgs_update_type': 'Damped',
    'tol': 1e-8,
    'maxiter': 25,
    'norm_type': 'L1',
    'barrier_strategy': 'Complementarity fraction',
    'start_strategy': 'Affine step'}

prefix = 'results'
optimization_options['output_file'] = os.path.join(prefix, 'output_file.dat')
optimization_options['tr_output_file'] = os.path.join(prefix, 'tr_output_file.dat')

# Set the communicator
comm = MPI.COMM_WORLD

# Set the type of filter to use
filter_type = 'lagrange'

order = 2 # Order of the mesh
nlevels = 4 # Number of multigrid levels
forest = create_forest(comm, nlevels-1)

# Set the boundary conditions for the problem
bcs = TMR.BoundaryConditions()
bcs.addBoundaryCondition('fixed')

# Create the material properties
density = 2600.0
rho = [density]
E = [70e9]
nu = [0.3]
props = TMR.StiffnessProperties(rho, E, nu)

# Set the original filter to NULL
orig_filter = None
xopt = None

max_iterations = 2
for step in range(max_iterations):
    # Create the problem
    problem = create_problem(forest, bcs, props, nlevels)
    problem.setPrefix(prefix)

    # Extract the filter to interpolate design variables
    filtr = problem.getFilter()

    if orig_filter is not None:
        # Create one of the new design vectors
        x = problem.createDesignVec()
        TopOptUtils.interpolateDesignVec(orig_filter, xopt, filtr, x)
        problem.setInitDesignVars(x)

    orig_filter = filtr

    # Optimize
    opt = TopOptUtils.TopologyOptimizer(problem, optimization_options)
    xopt = opt.optimize()

    # Refine based solely on the value of the density variable
    assembler = problem.getAssembler()
    TopOptUtils.densityBasedRefine(forest, assembler, lower=0.05, upper=0.5)

    # Repartition the mesh
    forest.repartition()

# Output for visualization
flag = (TACS.ToFH5.NODES |
        TACS.ToFH5.DISPLACEMENTS |
        TACS.ToFH5.STRAINS |
        TACS.ToFH5.STRESSES |
        TACS.ToFH5.EXTRAS)
assembler = problem.getAssembler()
f5 = TACS.ToFH5(assembler, TACS.PY_SOLID, flag)
f5.writeToFile(os.path.join(prefix, 'cantilever.f5'))
