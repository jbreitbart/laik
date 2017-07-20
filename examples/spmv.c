/* This file is part of the LAIK parallel container library.
 * Copyright (c) 2017 Josef Weidendorfer
 *
 * LAIK is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3.
 *
 * LAIK is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SPMV example.
 */

#include "laik.h"

#ifdef USE_MPI
#include "laik-backend-mpi.h"
#else
#include "laik-backend-single.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define SIZE 10000

typedef struct _SpM SpM;
struct _SpM {
    int rows, cols;
    int elems;
    int row[SIZE+1];
    int* col;
    double* val;
};

// for element-wise weighted partitioning: number of elems in row
double getEW(Laik_Index* i, void* d)
{
    SpM* m = (SpM*) d;
    int ii = i->i[0];

    return (double) (m->row[ii + 1] - m->row[ii]);
}

int main(int argc, char* argv[])
{
#ifdef USE_MPI
    Laik_Instance* inst = laik_init_mpi(&argc, &argv);
#else
    Laik_Instance* inst = laik_init_single();
#endif
    Laik_Group* world = laik_world(inst);

    laik_set_phase(inst, 0, "init", NULL);

    // generate diagonal matrix in CSR format
    SpM* m = (SpM*) malloc(sizeof(SpM));
    m->rows  = SIZE;
    m->cols  = SIZE;
    m->elems = (m->rows-1) * m->cols / 2;
    m->col   = (int*) malloc(m->elems * sizeof(int));
    m->val   = (double*) malloc(m->elems * sizeof(double));
    int r, off = 0;
    for(r = 0; r < SIZE; r++) {
        m->row[r] = off;
        for(int c = 0; c < r; c++) {
            m->col[off] = c;
            m->val[off] = (double) (SIZE - r);
            off++;
        }
    }
    m->row[r] = off;
    assert(m->elems == off);

    // global vector
    double* v = (double*) malloc(SIZE * sizeof(double));
    for(int i = 0; i < SIZE; i++)
        v[i] = (double) (i + 1);

    // 1d space for matrix rows and vector <res>
    Laik_Space* s = laik_new_space_1d(inst, SIZE);
    // result vector
    Laik_Data* resD = laik_alloc(world, s, laik_Double);

    // block partitioning according to elems in matrix rows
    Laik_Partitioning* p = laik_new_base_partitioning(world, s, LAIK_PT_Block);
    Laik_AccessPhase* ap = laik_new_accessphase(p, LAIK_DF_CopyOut);
    laik_set_index_weight(laik_get_partitioner(p), getEW, m);
    laik_switch_to_accessphase(resD, ap);

    double* res;
    uint64_t count;
    Laik_Slice* slc;
    int fromRow, toRow;

    // do SPMV, first time

    laik_set_phase(inst, 1, "1st SpmV", NULL);
    // init result vector (only my partition)
    laik_map_def1(resD, (void**) &res, &count);
    for(uint64_t i = 0; i < count; i++)
        res[i] = 0.0;
    // SPMV on my part of matrix rows
    slc = laik_my_slice(p, 0);
    fromRow = slc->from.i[0];
    toRow = slc->to.i[0];
    for(int r = fromRow; r < toRow; r++) {
        for(int o = m->row[r]; o < m->row[r+1]; o++)
            res[r - fromRow] += m->val[o] * v[m->col[o]];
        laik_set_iteration(inst, r-fromRow);
    }
    // push result to master
    laik_switchto_new_accessphase(resD, LAIK_PT_Master, LAIK_DF_CopyIn);
    if (laik_myid(world) == 0) {
        laik_map_def1(resD, (void**) &res, &count);
        double sum = 0.0;
        for(uint64_t i = 0; i < count; i++) sum += res[i];
        printf("Res sum (regular): %f\n", sum);
    }

    
    laik_iter_reset(inst);
    laik_set_phase(inst, 2, "2nd SpmV", NULL);

    // do SPMV, second time

    // other way to push results to master: use sum reduction
    laik_switchto_new_accessphase(resD, LAIK_PT_All,
                                  LAIK_DF_Init|LAIK_DF_ReduceOut|LAIK_DF_Sum);
    laik_map_def1(resD, (void**) &res, &count);
    slc = laik_my_slice(p, 0);
    fromRow = slc->from.i[0];
    toRow = slc->to.i[0];
    for(int r = fromRow; r < toRow; r++) {
        for(int o = m->row[r]; o < m->row[r+1]; o++)
            res[r] += m->val[o] * v[m->col[o]];
        laik_set_iteration(inst, r-fromRow);
    }
    laik_switchto_new_accessphase(resD, LAIK_PT_Master, LAIK_DF_CopyIn);
    if (laik_myid(world) == 0) {
        laik_map_def1(resD, (void**) &res, &count);
        double sum = 0.0;
        for(uint64_t i = 0; i < count; i++) sum += res[i];
        printf("Res sum (reduce): %f\n", sum);
    }

    laik_finalize(inst);
    return 0;
}
