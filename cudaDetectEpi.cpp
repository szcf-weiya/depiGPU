#include<iostream>
#include<iomanip>
#include<fstream>
#include<sstream>
#include<vector>
#include<string>
#include<stdlib.h>
#include<stdio.h>
#include<time.h>
clock_t start, stop;
#include<gsl/gsl_vector.h>
#include<gsl/gsl_matrix.h>
#include<gsl/gsl_combination.h>
#include<gsl/gsl_statistics.h>
#include<gsl/gsl_fit.h>
#include<gsl/gsl_multifit.h>
#include<gsl/gsl_cdf.h>
#include<omp.h>

#include "cuMultifit.h"

using namespace std;

#define MAX_ROW 305
#define MAX_COL 5000
#define COV_COL 4
#define MAX_PAIR_CORR 0.98
#define MAX_EPS 1e+12
#define IS_EXIST_COV 1
#define MIN_P_VALUE 0.001


#define coef(i) (gsl_vector_get(coef, (i)))
//#define COV(i, j) (gsl_matrix_get(cov, (i), (j)))

void readData(char* FILE, gsl_matrix *m, int row, int col, vector<string> &rowname, vector<string> &colname)
{
  ifstream input(FILE);
  string tmp, tmpp;
  //  vector<string> rowname;
  //vector<string> colname;

  double val;
  // colnames
  string line;
  // the first line
  getline(input, line);
  stringstream ss(line);
  // ss >> tmp; // null no need!!
  //  cout << tmp << endl;
  for (size_t i = 0; i < col; i++)
    {
      ss >> tmp;
      tmpp = tmp.substr(1, tmp.size() - 2);
      colname.push_back(tmpp);
    }

  for (size_t i = 0; i < row; i++)
    {
      getline(input, line);
      stringstream ss(line);
      ss >> tmp; // rowname
      rowname.push_back(tmp);
      for (size_t j = 0; j < col; j++)
	{
	  ss >> tmp;
	  val = strtod(tmp.c_str(), NULL); // transfer str to double
	  gsl_matrix_set(m, i, j, val);
	}
    }
  input.close();
}

int main()
{
  start = clock();
  /////////////////////////////////////
  //
  //  read data G
  //
  /////////////////////////////////////
  FILE *output;
  output = fopen("res_gsl.txt", "w");
  char FILE[] = "Data.txt";
  gsl_matrix *G = gsl_matrix_alloc(MAX_ROW, MAX_COL);
  vector<string> G_colname;
  vector<string> G_rowname;
  readData(FILE, G, MAX_ROW, MAX_COL, G_rowname, G_colname);
  //  ifstream input3("cov.txt");
  char FILE3[] = "cov.txt";
  gsl_matrix *coveriate = gsl_matrix_alloc(MAX_ROW, COV_COL);
  vector<string> colname;
  vector<string> rowname;
  readData(FILE3, coveriate, MAX_ROW, COV_COL, rowname, colname);
  /////////////////////////////////
  //
  //   read data Y.FA
  //
  /////////////////////////////////

  ifstream input2("Y.FA.txt");
  double val;
  gsl_vector *Y = gsl_vector_alloc(MAX_ROW);
  for (size_t i = 0; i < MAX_ROW; i++)
    {
      input2 >> val;
      gsl_vector_set(Y, i, val);
    }
  input2.close();
  ///////////////////////////////////////
  //
  // combination
  //
  //////////////////////////////////////
  /*
  gsl_combination *c;
  vector<size_t> r1, r2;
  //  size_t k = 2;
  size_t *res;
  res = (size_t*)malloc(sizeof(size_t)*2);
  c = gsl_combination_calloc(MAX_COL, 2);
  do{
    res = gsl_combination_data(c);
    r1.push_back(res[0]);
    r2.push_back(res[1]);
  }while(gsl_combination_next(c) == GSL_SUCCESS);
  cout << r1.size() << " " << r2.size() << endl;
  */
  ////////////////////////////////////////
  //
  //  main
  //
  ////////////////////////////////////////
  gsl_vector *x0 = gsl_vector_alloc(MAX_ROW);
  gsl_vector_set_all(x0, 1);
  //  gsl_vector *x1_copy = gsl_vector_alloc(MAX_ROW);

  int p;
  int max_val, min_val;

  gsl_vector *coveriate_vec;
  coveriate_vec = gsl_vector_alloc(MAX_ROW);
  if (!IS_EXIST_COV)
      p = 4;
  else
      p = 4 + COV_COL;
  gsl_matrix *X;
  X = gsl_matrix_alloc(MAX_ROW, p);
  if (IS_EXIST_COV)
    {
        for (int ip = 0; ip < COV_COL; ip++)
	  {
	    gsl_matrix_get_col(coveriate_vec, coveriate, ip);
	    gsl_matrix_set_col(X, 4+ip, coveriate_vec);
	  }
    }
  gsl_vector_free(coveriate_vec);
  gsl_matrix_free(coveriate);

  // cout << "  Number of processors available = " << omp_get_num_procs ( ) << "\n";
  // cout << "  Number of threads =              " << omp_get_max_threads ( ) << "\n";

  fprintf(output, "r1\tr2\tr1.p.value\tr2.p.value\tr1*r2.p.value\n");
  //# pragma omp parallel
  //# pragma omp for schedule(dynamic) // relatively slow
  gsl_matrix_set_col(X, 0, x0);
  gsl_vector *x1 = gsl_vector_alloc(MAX_ROW);
  for (int i = 0; i < MAX_COL/100; i++)
    {
      # pragma omp sections
      {
	# pragma omp section
	gsl_matrix_get_col(x1, G, i);
	# pragma omp section
	max_val = gsl_vector_max(x1);
	# pragma omp section
	min_val = gsl_vector_min(x1);
      }
      if (max_val == min_val)
	continue;
      gsl_matrix_set_col(X, 1, x1);
      # pragma omp parallel for schedule(dynamic) // faster!!!
      for (int j = i+1; j < MAX_COL; j++)
	{
	  gsl_vector *x2 = gsl_vector_alloc(MAX_ROW);

	  size_t flag;

	  gsl_matrix_get_col(x2, G, j);
	  // if same
	  max_val = gsl_vector_max(x2);
	  min_val = gsl_vector_min(x2);
	  if (max_val == min_val)
	    {
	      gsl_vector_free(x2);
	      continue;
	    }
	  gsl_matrix_set_col(X, 2, x2);

	  double res_corr = (float)gsl_stats_correlation(x1->data, 1, x2->data, 1, MAX_ROW);
	  if (fabs(res_corr) >= MAX_PAIR_CORR)
	    continue;
	  gsl_vector *x3 = gsl_vector_alloc(MAX_ROW);
	  gsl_matrix_get_col(x3, G, i);
	  gsl_vector_mul(x3, x2);
	  gsl_vector_free(x2);

	  max_val = gsl_vector_max(x3);
	  //	  min_val = gsl_vector_min(x3);
	  if (max_val == 0)
	    {
	      gsl_vector_free(x3);
	      continue;
	    }

	  gsl_matrix_set_col(X, 3, x3);
	  gsl_vector_free(x3);

    double *pvalue, *coef;
    pvalue = (double*)malloc(sizeof(double)*p);
    coef = (double*)malloc(sizeof(double)*p);

    cuMultifit(X->data, MAX_ROW, p, Y->data, coef, pvalue);

	  flag = 1;
	  for (int k = 1; k < p; k++){
	    if (coef[k] > MAX_EPS)
	      flag = 0;
	  }
    free(coef);
	  if (flag == 0)
    {
      free(pvalue);
      continue;
    }
	  if (pvalue[2] > MIN_P_VALUE)
    {
      free(pvalue);
      continue;
    }
    
	  fprintf(output, "%d\t%d\t%.6f\t%.6f\t%.10f\n", i, j, pvalue[0], pvalue[1], pvalue[2]);
	}
  }
  gsl_vector_free(x0);
  gsl_vector_free(x1);
  gsl_matrix_free(X);
  gsl_vector_free(Y);
  gsl_matrix_free(G);
  fclose(output);
  //  output.close();
  return 0;
}
