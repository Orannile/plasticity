//FE parameters
#define feOrder   1 // Basis function interpolation order (1-linear)
#define quadOrder 2 // Quadrature point order n^3 (2->8 quadrature points)
#define meshRefineFactor 3 // 2^n*2^n*2^n elements(3->8*8*8 =512 elements)

//Solution output parameters
#define writeOutput true // flag to write output vtu and pvtu files
#define outputDirectory "."
 
#define linearSolverType PETScWrappers::SolverCG // Type of linear solver
#define totalNumIncrements 100 // No. of increments
#define maxLinearSolverIterations 50000 // Maximum iterations for linear solver
#define relLinearSolverTolerance  1.0e-10 // Relative linear solver tolerance
#define maxNonLinearIterations 5 // Maximum no. of non-linear iterations
#define absNonLinearTolerance 1.0e-18 // Non-linear solver tolerance
#define relNonLinearTolerance 1.0e-6 // Relative non-linear solver tolerance
#define stopOnConvergenceFailure false // Flag to stop problem if convergence fails

//Elastic Parameters
#define c11  170e3 // C11 (MPa)
#define c12 124e3 // C12 (MPa)
#define c44 75e3  // C44 (MPa)

//Crystal Plasticity parameters
#define numSlipSystems 12 // generally 12 for FCC
#define latentHardeningRatio 1.4  //q1
#define powerLawExponent 2.25  //a
#define initialSlipResistance 16.0 // CRSS s0(MPa)
#define saturationStress 148.0 //s_s(MPa)
#define initialHardeningModulus 180.0 //s_s(MPa) //h0(MPa)

// Read Input Microstructure
unsigned int numPts[3]={20, 20, 22}; // No. of voxels in x,y and z directions






