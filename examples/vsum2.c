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
 * Vector sum example (2).
 *
 * Same as vsum, but using 2-cyclic block paritioning; that is, every
 * task will get to ranges in the block partitiongs. To this end, we
 * set the cycle count for the block partitioner to 2.
 */

#include "laik.h"

#ifdef USE_MPI
#include "laik-backend-mpi.h"
#else
#include "laik-backend-single.h"
#endif

#include <stdio.h>
#include <assert.h>

// for element-wise weighted partitioning: same as index
double getEW(Laik_Index* i, void* d) { return (double) i->i[0]; }
// for task-wise weighted partitioning: skip task given as user data
double getTW(int r, void* d) { return ((long int)d == r) ? 0.0 : 1.0; }

int main(int argc, char* argv[])
{
#ifdef USE_MPI
    Laik_Instance* inst = laik_init_mpi(&argc, &argv);
#else
    Laik_Instance* inst = laik_init_single();
#endif
    Laik_Group* world = laik_world(inst);

    laik_set_phase(inst, 0, "init", NULL);

    double *base;
    uint64_t count;

    // different partitionings used
    Laik_AccessPhase *ap2, *ap3, *ap4;
    Laik_Partitioning *p2, *p3, *p4;
    Laik_Mapping* m;

    // do partial sums using different partitionings
    double mysum[4] = { 0.0, 0.0, 0.0, 0.0 };

    // allocate global 1d double array: 1 mio entries
    Laik_Data* a = laik_alloc_1d(world, laik_Double, 1000000);

    laik_set_phase(inst, 1, "master-only", NULL);

    // initialize at master (others do nothing, empty partition)
    laik_switchto_new_accessphase(a, LAIK_PT_Master, LAIK_DF_CopyOut);
    if (laik_myid(world) == 0) {
        // it is ensured this is exactly one slice
        m = laik_map_def1(a, (void**) &base, &count);
        for(uint64_t i = 0; i < count; i++) base[i] = (double) i;
    }
    // partial sum (according to master partitioning)
    laik_map_def1(a, (void**) &base, &count);
    for(uint64_t i = 0; i < count; i++) mysum[0] += base[i];

    laik_set_phase(inst, 2, "block", NULL);

    // distribute data equally among all
    p2 = laik_new_base_partitioning(laik_get_group(a), laik_get_space(a),
                                    LAIK_PT_Block);
    laik_set_cycle_count(laik_get_partitioner(p2), 2);
    ap2 = laik_new_accessphase(p2, LAIK_DF_CopyIn | LAIK_DF_CopyOut);
    laik_switch_to_accessphase(a, ap2);
    // partial sum using equally-sized blocks, outer loop over slices
    for(int sNo = 0;; sNo++) {
        if (laik_map_def(a, sNo, (void**) &base, &count) == 0) break;
        for(uint64_t i = 0; i < count; i++) mysum[1] += base[i];
    }

    laik_set_phase(inst, 3, "element-wise", NULL);

    // distribution using element-wise weights equal to index
    p3 = laik_new_base_partitioning(laik_get_group(a), laik_get_space(a),
                                    LAIK_PT_Block);
    laik_set_cycle_count(laik_get_partitioner(p3), 2);
    laik_set_index_weight(laik_get_partitioner(p3), getEW, 0);
    ap3 = laik_new_accessphase(p3, LAIK_DF_CopyIn | LAIK_DF_CopyOut);
    laik_switch_to_accessphase(a, ap3);
    // partial sum using blocks sized by element weights
    for(int sNo = 0;; sNo++) {
        if (laik_map_def(a, sNo, (void**) &base, &count) == 0) break;
        for(uint64_t i = 0; i < count; i++) mysum[2] += base[i];
    }

    laik_set_phase(inst, 3, "task-wise", NULL);

    if (laik_size(world) > 1) {
        // distribution using task-wise weights: without master
        p4 = laik_new_base_partitioning(laik_get_group(a), laik_get_space(a),
                                        LAIK_PT_Block);
        laik_set_cycle_count(laik_get_partitioner(p4), 2);
        laik_set_task_weight(laik_get_partitioner(p4), getTW, 0); // without master
        ap4 = laik_new_accessphase(p4, LAIK_DF_CopyIn | LAIK_DF_CopyOut);
        laik_switch_to_accessphase(a, ap4);
        // partial sum using blocks sized by task weights
        for(int sNo = 0;; sNo++) {
            if (laik_map_def(a, sNo, (void**) &base, &count) == 0) break;
            for(uint64_t i = 0; i < count; i++) mysum[3] += base[i];
        }
    }
    else
        mysum[3] = mysum[0];

    printf("Id %d: partitial sums %.0f, %.0f, %.0f, %.0f\n",
           laik_myid(world), mysum[0], mysum[1], mysum[2], mysum[3]);

    laik_set_phase(inst, 5, "verification", NULL);

    // for collecting partial sums at master, use LAIK's automatic
    // aggregation functionality when switching to new partitioning
    Laik_Data* sum = laik_alloc_1d(world, laik_Double, 4);
    laik_switchto_new_accessphase(sum, LAIK_PT_All,
                                  LAIK_DF_ReduceOut | LAIK_DF_Sum);
    laik_map_def1(sum, (void**) &base, &count);
    assert(count == 4);
    for(int i = 0; i < 4; i++) base[i] = mysum[i];

    // master-only partitioning: add partial values to be read at master
    laik_switchto_new_accessphase(sum, LAIK_PT_Master, LAIK_DF_CopyIn);
    if (laik_myid(world) == 0) {
        laik_map_def1(sum, (void**) &base, &count);
        printf("Total sums: %.0f, %.0f, %.0f, %.0f\n",
               base[0], base[1], base[2], base[3]);
    }

    laik_finalize(inst);
    return 0;
}
