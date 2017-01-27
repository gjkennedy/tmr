#ifndef TMR_BASE_H
#define TMR_BASE_H

/*
  Copyright (c) 2016 Graeme Kennedy. All rights reserved. 
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"

/*
  The following constants define the maximum octant depth and maximum
  element order within the code.
*/
static const int TMR_MAX_LEVEL = 30;

/*
  Base class for all point-evaluation algorithms
*/
class TMRPoint {
 public:
  TMRPoint(){}
  TMRPoint( const TMRPoint& p ){
    x = p.x;  y = p.y;  z = p.z;
  }
  TMRPoint( double _x, double _y, double _z ){
    x = _x;  y = _y;  z = _z;
  }
  inline void zero(){
    x = y = z = 0.0;
  }
  inline double dot( const TMRPoint& p ) const {
    return x*p.x + y*p.y + z*p.z;
  }
  
  double x, y, z;
};

/*
  The MPI TMROctant data type
*/
extern MPI_Datatype TMROctant_MPI_type;
extern MPI_Datatype TMRQuadrant_MPI_type;
extern MPI_Datatype TMRPoint_MPI_type;

// Initialize and finalize the data type
void TMRInitialize();
void TMRFinalize();

/*
  The following class is used to help create the interpolation and
  restriction operators. It stores both the node index and
  interpolation/restriction weight values in the same structure.
*/
class TMRIndexWeight {
 public:
  TMRIndexWeight(){}
  TMRIndexWeight( const TMRIndexWeight& in ){
    index = in.index;  weight = in.weight;
  }
  int index;
  double weight;

  // Sort and uniquify a list of indices and weights
  static int uniqueSort( TMRIndexWeight *array, int size ){
    qsort(array, size, sizeof(TMRIndexWeight), 
	  TMRIndexWeight::compare_weights);
    
    // Now that the weights are sorted, remove duplicates by adding up
    // the weights
    int j = 0; // Location to place entries
  
    for ( int i = 0; i < size; i++, j++ ){
      // Copy over the newest weight/index data
      if (i != j){
        array[j] = array[i];
      }
      
      // While the index is the same as the next, keep
      // adding the weight
      while ((i < size-1) && 
             (array[i].index == array[i+1].index)){
        array[j].weight += array[i+1].weight;
        i++;
      }
    }
    
    // Return the new size of the array
    return j;
  }

 private:
  // Compare two TMRIndexWeight objects - compare them based on
  // their index value only.
  static int compare_weights( const void *a, const void *b ){
    const TMRIndexWeight *A = static_cast<const TMRIndexWeight*>(a);
    const TMRIndexWeight *B = static_cast<const TMRIndexWeight*>(b);
    
    return A->index - B->index;
  }
};

/*
  Reference counted TMR entity
*/
class TMREntity {
 public:
  TMREntity(){
    ref_count = 0;
  }
  virtual ~TMREntity(){}
  
  // Reference count the geometric entity objects
  // --------------------------------------------
  void incref();
  void decref();

  // Set/get the tolerances used within the geometric search algorithms
  // ------------------------------------------------------------------
  void setTolerances( double _eps_dist, double _eps_cosine );
  void getTolerances( double *_eps_dist, double *_eps_cosine );

 protected:
  static double eps_dist;
  static double eps_cosine;

 private:
  // Total reference count for this object
  int ref_count;
};

#endif // TMR_BASE_H
