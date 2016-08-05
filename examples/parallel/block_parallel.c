#include "TMROctForest.h"
#include "TACSMeshLoader.h"

void getLocation( int i, int nx, int ny, int nz,
                  int x, int y, int z, double X[] ){
  int iz = i/(nx*ny);
  int iy = (i - iz*nx*ny)/nx;
  int ix = i % nx;

  const int32_t hmax = 1 << TMR_MAX_LEVEL;
  double u = (1.0*x)/hmax;
  double v = (1.0*y)/hmax;
  double w = (1.0*z)/hmax;

  X[0] = ix + u;
  X[1] = iy + v;
  X[2] = iz + w;
}

void getLocation( int i, const int *elem_node_conn, 
                  const double *Xpts,
                  int x, int y, int z, double X[] ){

  const int32_t hmax = 1 << TMR_MAX_LEVEL;
  double u = (1.0*x)/hmax;
  double v = (1.0*y)/hmax;
  double w = (1.0*z)/hmax;

  double N[8];
  N[0] = (1.0 - u)*(1.0 - v)*(1.0 - w);
  N[1] = u*(1.0 - v)*(1.0 - w);
  N[2] = (1.0 - u)*v*(1.0 - w);
  N[3] = u*v*(1.0 - w);
  N[4] = (1.0 - u)*(1.0 - v)*w;
  N[5] = u*(1.0 - v)*w;
  N[6] = (1.0 - u)*v*w;
  N[7] = u*v*w;

  X[0] = X[1] = X[2] = 0.0;
  for ( int k = 0; k < 8; k++ ){
    int node = elem_node_conn[8*i + k];
    
    X[0] += Xpts[3*node]*N[k];
    X[1] += Xpts[3*node+1]*N[k];
    X[2] += Xpts[3*node+2]*N[k];
  }
}

int main( int argc, char *argv[] ){
  MPI_Init(&argc, &argv);
  TMRInitialize();

  int partition = 0;
  for ( int k = 0; k < argc; k++ ){
    if (strcmp(argv[k], "partition") == 0){
      partition = 1;
    }
  }

  MPI_Comm comm = MPI_COMM_WORLD;
  TMROctForest *forest = new TMROctForest(comm);

  int mpi_rank;
  MPI_Comm_rank(comm, &mpi_rank);

  // Create the TACSMeshLoader class
  TACSMeshLoader *mesh = new TACSMeshLoader(MPI_COMM_SELF);
  mesh->incref();
  mesh->scanBDFFile("uCRM_3D_box_mesh.bdf");

  // Extract the connectivity
  int nnodes, nelems;
  const int *elem_node_conn;
  const double *Xpts;
  mesh->getConnectivity(&nnodes, &nelems, NULL, 
                        &elem_node_conn, &Xpts);
  forest->setConnectivity(nnodes, elem_node_conn, 
                          nelems, partition);

  // Set the refinement increasing out the wing
  int *refine = new int[ nelems ];
  memset(refine, 0, nelems*sizeof(int));

  int max_refine = 5;
  int min_refine = 2;
  double y_max = 30.0;
  for ( int k = 0; k < nelems; k++ ){
    double y_ref = Xpts[3*elem_node_conn[8*k]+1];
    refine[k] = min_refine + 
      (max_refine - min_refine)*(1.0 - (y_ref/y_max));
  }
  forest->createTrees(refine);
  delete [] refine;

  // Repartition the octrees
  printf("[%d] Repartition\n", mpi_rank);
  forest->repartition();

  printf("[%d] Balance\n", mpi_rank);
  double tbal = MPI_Wtime();
  forest->balance(1);
  tbal = MPI_Wtime() - tbal;

  printf("[%d] Create nodes\n", mpi_rank);
  double tnodes = MPI_Wtime();
  forest->createNodes(2);
  tnodes = MPI_Wtime() - tnodes;

  // Get the octrees within the forest
  TMROctree **trees;
  int ntrees = forest->getOctrees(&trees);

  // Create the mesh
  double tmesh = MPI_Wtime();
  int *conn, nfe = 0;
  forest->createMeshConn(&conn, &nfe);
  tmesh = MPI_Wtime() - tmesh;
  
  int ntotal = 0;
  MPI_Allreduce(&nfe, &ntotal, 1, MPI_INT, MPI_SUM, comm);

  // Get the rank
  int rank;
  MPI_Comm_rank(comm, &rank);

  if (rank == 0){
    printf("balance:  %15.5f s\n", tbal);
    printf("nodes:    %15.5f s\n", tnodes);
    printf("mesh:     %15.5f s\n", tmesh);
    printf("nelems:   %15d\n", ntotal);
  }

















  /*
  // Write out a file for each processor - bad practice!
  char filename[128];
  sprintf(filename, "parallel%d.dat", rank);
  FILE *fp = fopen(filename, "w");

  // Write the tecplot header
  fprintf(fp, "Variables = X, Y, Z, dv\n");

  for ( int i = 0; i < ntrees; i++ ){
    if (trees[i]){
      TMROctantArray *elements, *nodes;
      trees[i]->getNodes(&nodes);
      trees[i]->getElements(&elements);

      // Get the elements
      int size;
      TMROctant *array;
      elements->getArray(&array, &size);

      fprintf(fp, "ZONE T=TMR%d N=%d E=%d ", i, 8*size, size);
      fprintf(fp, "DATAPACKING=POINT ZONETYPE=FEBRICK\n");

      // Write out this portion of the forrest
      for ( int k = 0; k < size; k++ ){
        int32_t h = 1 << (TMR_MAX_LEVEL - array[k].level);
        int32_t x = array[k].x;
        int32_t y = array[k].y;
        int32_t z = array[k].z;

        TMROctant node;
        double X[3];
        for ( int kz = 0; kz < 2; kz++ ){
          for ( int ky = 0; ky < 2; ky++ ){
            for ( int kx = 0; kx < 2; kx++ ){
              node.x = x + h*kx;
              node.y = y + h*ky;
              node.z = z + h*kz;
              const int use_node_search = 1;
              TMROctant *t = nodes->contains(&node, use_node_search);

              // getLocation(i, nx, ny, nz,
              //             x + kx*h, y + ky*h, z + kz*h, X);
              getLocation(i, elem_node_conn, Xpts,
                          x + kx*h, y + ky*h, z + kz*h, X);
              fprintf(fp, "%e %e %e %d\n", X[0], X[1], X[2], t->tag);
            }
          }
        }
      }

      for ( int k = 0; k < size; k++ ){
	fprintf(fp, "%d %d %d %d %d %d %d %d\n",
                8*k+1, 8*k+2, 8*k+4, 8*k+3,
                8*k+5, 8*k+6, 8*k+8, 8*k+7);
      }
    }
  }

  fclose(fp);
  */
  delete forest;

  TMRFinalize();
  MPI_Finalize();
  return (0);
}
