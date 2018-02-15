/*
 * This file is part of the LAIK parallel container library.
 * Copyright (c) 2018 Jens Breitbart, Josef Weidendorfer
 */

#include "laik-internal.h"
#include "laik-backend-gaspi.h"

#include "/home/jbreitbart/bin/gpi2/include/GASPI.h"

#include <unistd.h>
#include <stdio.h>
#include <assert.h>

// Prototypes
static void laik_gaspi_finalize();
static void laik_gaspi_updateGroup(Laik_Group* );
static Laik_TransitionPlan* laik_gaspi_prepare(Laik_Data* , Laik_Transition* );
static void laik_gaspi_cleanup(Laik_TransitionPlan* );
static void laik_gaspi_wait(Laik_TransitionPlan* , int);
static bool laik_gaspi_probe(Laik_TransitionPlan* , int);
static void laik_gaspi_exec(Laik_Data *, Laik_Transition *, Laik_TransitionPlan*,
                   Laik_MappingList* , Laik_MappingList* );

// C guarantees that unset function pointers are NULL
static Laik_Backend laik_backend_gaspi = {
    .name        = "GASPI Backend Driver",
    .finalize    = laik_gaspi_finalize,
    .prepare     = laik_gaspi_prepare,
    .cleanup     = laik_gaspi_cleanup,
    .exec        = laik_gaspi_exec,
    .wait        = laik_gaspi_wait,
    .probe       = laik_gaspi_probe,
    .updateGroup = laik_gaspi_updateGroup
};

Laik_Instance* laik_init_gaspi() {
    gaspi_return_t err;

    gaspi_config_t config;

    err = gaspi_config_get(&config);
    if (err != GASPI_SUCCESS) laik_log(5, "GASPI: Could not get config: %d", err);

    // TODO change gaspi config?
    config.notification_num=1024;

    err = gaspi_config_set(config);
    if (err != GASPI_SUCCESS) laik_log(5, "GASPI: Could not set config: %d", err);

    err = gaspi_proc_init(GASPI_BLOCK);
    if (err != GASPI_SUCCESS) laik_log(5, "GASPI: Could not inititalize: %d", err);

    gaspi_rank_t rank, size;

    err = gaspi_proc_num(&size);
    if (err != GASPI_SUCCESS) laik_log(5, "GASPI: Could get number of processes: %d", err);

    err = gaspi_proc_rank(&rank);
    if (err != GASPI_SUCCESS) laik_log(5, "GASPI: Could get rank: %d", err);

    char hostname[1024];
    gethostname(hostname, 1024);

    Laik_Instance* inst = laik_new_instance(&laik_backend_gaspi, size, rank, hostname, 0, 0);

    sprintf(inst->guid, "%d", rank);

    laik_log(1, "GASPI backend initialized (location '%s', pid %d)\n",
             inst->mylocation, (int) getpid());

    return inst;
}

static void laik_gaspi_finalize() {
    gaspi_return_t err;

    err = gaspi_proc_term(GASPI_BLOCK);
    if (err != GASPI_SUCCESS) laik_log(5, "GASPI: Could not terminate: %d", err);
}

struct _Laik_TransitionPlan {
    //Laik_Data* data;
    //Laik_Transition* transition;
};

// Prepare the repeated asynchronous execution of a transition on data
//
// can be NULL to state that this communication backend driver does not
// support transition plans. This implies that only synchronous transitions
// are supported and executed by calling exec.
// if NULL, LAIK will never call cleanup/wait/probe (can be NULL, too)
//
// this function allows the backend driver to
// - allocate resources which can be reused when doing the same transition
//   multiple times (such as memory space for buffers, reductions, lists
//   of outstanding requests, or resources of the communication library).
//   For one LAIK data container, only one asynchronous transition can be
//   active at any time. Thus, resources can be shared among transition
//   plans for one data container.
// - prepare an optimized communcation schedule using the pre-allocated
//   resources
//
// TODO:
// - how to enable merging of plans for different data doing same transition
// - for reserved and pre-allocated mappings (and fixed data layout),
//   we could further optimize/specialize our plan:
//   => extend by providing from/to mappings (instead of providing in exec)
static Laik_TransitionPlan* laik_gaspi_prepare(Laik_Data* d, Laik_Transition* t) {
    // no identifier provided for t? We need to check all elements of the tansition
    // to be sure it is the same as before.

    // currently we are doing nothing here
    (void)d;
    (void)t;
    return 0;
}

// free resources allocated for a transition plan
static void laik_gaspi_cleanup(Laik_TransitionPlan* p) {
    (void)p;
    // to nothing on purpose
}

// wait for outstanding asynchronous communication requests resulting from
// a call to exec when using a transition plan.
// If a LAIK container uses multiple mappings, you can wait for finished
// communication for each mapping separately.
// Use -1 for <mapNo> to wait for all.
static void laik_gaspi_wait(Laik_TransitionPlan* p, int mapNo) {
    // required due to interface signature
    (void) p;
    (void) mapNo;

    // nothing to wait for: this backend driver currently is synchronous
}

// similar to wait, but just probe if any communication for a given mapping
// already finished (-1 for all)
static bool laik_gaspi_probe(Laik_TransitionPlan* p, int mapNo) {
    // required due to interface signature
    (void) p;
    (void) mapNo;

    // all communication finished: this backend driver currently is synchronous
    return true;
}

// update backend specific data for group if needed
static void laik_gaspi_updateGroup(Laik_Group* g) {
    (void)g;
}


// execute a transition by triggering required communication.
// a transition plan can be specified, allowing asynchronous execution.
// else, the transition will be executed synchronously (set <p> to 0).
// data to send is found in mappings in <from>, to receive in <to>
// before executing a transition plan again, call wait (see below)
static void laik_gaspi_exec(Laik_Data *data, Laik_Transition *t, Laik_TransitionPlan* p,
                   Laik_MappingList* fromList, Laik_MappingList* toList) {
   (void)p;

    Laik_Group* g = data->activePartitioning->group;
    int myid  = g->myid;

    laik_log(1, "GASPI backend execute transition:\n"
             "  data '%s', group %d (size %d, myid %d)\n"
             "  actions: %d reductions, %d sends, %d recvs",
             data->name, g->gid, g->size, myid,
             t->redCount, t->sendCount, t->recvCount);

    if (myid < 0) {
        // this task is not part of the communicator to use
        return;
    }
    #if 0

    int dims = data->space->dims;
    Laik_SwitchStat* ss = data->stat;

    MPIGroupData* gd = mpiGroupData(g);
    assert(gd); // must have been updated by laik_mpi_updateGroup()
    MPI_Comm comm = gd->comm;
    MPI_Status status;

    if (t->redCount > 0) {
        assert(dims == 1);
        assert(fromList);

        for(int i=0; i < t->redCount; i++) {
            struct redTOp* op = &(t->red[i]);
            int64_t from = op->slc.from.i[0];
            int64_t to   = op->slc.to.i[0];

            assert(op->myInputMapNo >= 0);
            assert(op->myInputMapNo < fromList->count);
            Laik_Mapping* fromMap = fromList->map[op->myInputMapNo];

            Laik_Mapping* toMap = 0;
            if (toList && (op->myOutputMapNo >= 0)) {
                assert(op->myOutputMapNo < toList->count);
                toMap = toList->map[op->myOutputMapNo];

                if (toMap->base == 0) {
                    laik_allocateMap(toMap, ss);
                    assert(toMap->base != 0);
                }
            }

            char* fromBase = fromMap ? fromMap->base : 0;
            char* toBase = toMap ? toMap->base : 0;
            uint64_t elemCount = to - from;
            uint64_t byteCount = elemCount * d->elemsize;

            assert(fromBase != 0);
            // if current task is receiver, toBase should be allocated
            if (laik_isInGroup(t, op->outputGroup, myid))
                assert(toBase != 0);
            else
                toBase = 0; // no interest in receiving anything

            assert(from >= fromMap->requiredSlice.from.i[0]);
            fromBase += (from - fromMap->requiredSlice.from.i[0]) * d->elemsize;
            if (toBase) {
                assert(from >= toMap->requiredSlice.from.i[0]);
                toBase += (from - toMap->requiredSlice.from.i[0]) * d->elemsize;
            }

            MPI_Datatype mpiDataType = getMPIDataType(d);

            // all-groups never should be specified explicitly
            if (op->outputGroup >= 0)
                assert(t->group[op->outputGroup].count < g->size);
            if (op->inputGroup >= 0)
                assert(t->group[op->inputGroup].count < g->size);

            // if neither input nor output are all-groups: manual reduction
            if ((op->inputGroup >= 0) && (op->outputGroup >= 0)) {

                // do the manual reduction on smallest rank of output group
                int reduceTask = t->group[op->outputGroup].task[0];

                laik_log(1, "Manual reduction at T%d: (%lld - %lld) slc/map %d/%d",
                         reduceTask, (long long int) from, (long long int) to,
                         op->myInputSliceNo, op->myInputMapNo);

                if (reduceTask == myid) {
                    TaskGroup* tg;

                    // collect values from tasks in input group
                    tg = &(t->group[op->inputGroup]);
                    // check that bufsize is enough
                    assert(tg->count * byteCount < PACKBUFSIZE);

                    char* ptr[32], *p;
                    assert(tg->count <= 32);
                    p = packbuf;
                    int myIdx = -1;
                    for(int i = 0; i< tg->count; i++) {
                        if (tg->task[i] == myid) {
                            ptr[i] = fromBase;
                            myIdx = i;

#ifdef LOG_DOUBLE_VALUES
                            assert(d->elemsize == 8);
                            for(uint64_t i = 0; i < elemCount; i++)
                                laik_log(1, "    have at %d: %f", from + i,
                                         ((double*)fromBase)[i]);
#endif
#ifdef LOG_FLOAT_VALUES
                            assert(d->elemsize == 4);
                            for(uint64_t i = 0; i < elemCount; i++)
                                laik_log(1, "    have at %d: %f", from + i,
                                         (double) ((float*)fromBase)[i] );
#endif
                            continue;
                        }

                        laik_log(1, "  MPI_Recv from T%d (buf off %lld)",
                                 tg->task[i], (long long int) (p - packbuf));

                        ptr[i] = p;
                        MPI_Recv(p, elemCount, mpiDataType,
                                 tg->task[i], 1, comm, &status);
#ifdef LOG_DOUBLE_VALUES
                        assert(d->elemsize == 8);
                        for(uint64_t i = 0; i < elemCount; i++)
                            laik_log(1, "    got at %d: %f", from + i,
                                     ((double*)p)[i]);
#endif
                        p += byteCount;
                    }

                    // toBase may be same as fromBase (= our values).
                    // e.g. when we are 3rd task (ptr[3] == fromBase), we
                    // would overwrite our values. Swap ptr[0] with ptr[3].
                    if (myIdx >= 0) {
                        assert(ptr[myIdx] == fromBase);
                        ptr[myIdx] = ptr[0];
                        ptr[0] = fromBase;
                    }

                    // do the reduction, put result back to my input buffer
                    if (d->type->reduce) {
                        assert(tg->count > 1);


                        (d->type->reduce)(toBase, ptr[0], ptr[1],
                                elemCount, op->redOp);
                        for(int t = 2; t < tg->count; t++)
                            (d->type->reduce)(toBase, toBase, ptr[t],
                                              elemCount, op->redOp);
                    }
                    else {
                        laik_log(LAIK_LL_Panic,
                                 "Need reduce function for type '%s'. Not set!",
                                 d->type->name);
                        assert(0);
                    }

#ifdef LOG_DOUBLE_VALUES
                    assert(d->elemsize == 8);
                    for(uint64_t i = 0; i < elemCount; i++)
                        laik_log(1, "    sum at %d: %f", from + i,
                                 ((double*)toBase)[i]);
#endif

                    // send result to tasks in output group
                    tg = &(t->group[op->outputGroup]);
                    for(int i = 0; i< tg->count; i++) {
                        if (tg->task[i] == myid) {
                            // that's myself: nothing to do
                            continue;
                        }

                        laik_log(1, "  MPI_Send result to T%d", tg->task[i]);

                        MPI_Send(toBase, elemCount, mpiDataType,
                                 tg->task[i], 1, comm);
                    }
                }
                else {
                    if (laik_isInGroup(t, op->inputGroup, myid)) {
                        laik_log(1, "  MPI_Send to T%d", reduceTask);

#ifdef LOG_DOUBLE_VALUES
                        assert(d->elemsize == 8);
                        for(uint64_t i = 0; i < elemCount; i++)
                            laik_log(1, "    at %d: %f", from + i,
                                     ((double*)fromBase)[i]);
#endif

                        MPI_Send(fromBase, elemCount, mpiDataType,
                                 reduceTask, 1, comm);
                    }
                    if (laik_isInGroup(t, op->outputGroup, myid)) {
                        laik_log(1, "  MPI_Recv from T%d", reduceTask);

                        MPI_Recv(toBase, elemCount, mpiDataType,
                                 reduceTask, 1, comm, &status);
#ifdef LOG_DOUBLE_VALUES
                        assert(d->elemsize == 8);
                        for(uint64_t i = 0; i < elemCount; i++)
                            laik_log(1, "    at %d: %f", from + i,
                                     ((double*)toBase)[i]);
#endif
                    }
                }
            }
            else {
                // not handled yet: either input or output is all-group
                assert((op->inputGroup == -1) || (op->outputGroup == -1));

                MPI_Op mpiRedOp;
                switch(op->redOp) {
                case LAIK_RO_Sum: mpiRedOp = MPI_SUM; break;
                default: assert(0);
                }

                int rootTask;
                if (op->outputGroup == -1) rootTask = -1;
                else {
                    // TODO: support more then 1 receiver
                    assert(t->group[op->outputGroup].count == 1);
                    rootTask = t->group[op->outputGroup].task[0];
                }

                if (laik_log_begin(1)) {
                    laik_log_append("MPI Reduce (root ");
                    if (rootTask == -1)
                        laik_log_append("ALL");
                    else
                        laik_log_append("%d", rootTask);
                    if (fromBase == toBase)
                        laik_log_append(", IN_PLACE");
                    laik_log_flush("): (%ld - %ld) in %d/%d out %d/%d (slc/map), "
                                   "elemsize %d, baseptr from/to %p/%p\n",
                                   from, to,
                                   op->myInputSliceNo, op->myInputMapNo,
                                   op->myOutputSliceNo, op->myOutputMapNo,
                                   d->elemsize, fromBase, toBase);
                }

#ifdef LOG_DOUBLE_VALUES
                if (fromBase) {
                    assert(d->elemsize == 8);
                    for(uint64_t i = 0; i < elemCount; i++)
                        laik_log(1, "    before at %d: %f", from + i,
                                 ((double*)fromBase)[i]);
                }
#endif

                if (rootTask == -1) {
                    if (fromBase == toBase)
                        MPI_Allreduce(MPI_IN_PLACE, toBase, to - from,
                                      mpiDataType, mpiRedOp, comm);
                    else
                        MPI_Allreduce(fromBase, toBase, to - from,
                                      mpiDataType, mpiRedOp, comm);
                }
                else {
                    if (fromBase == toBase)
                        MPI_Reduce(MPI_IN_PLACE, toBase, to - from,
                                   mpiDataType, mpiRedOp, rootTask, comm);
                    else
                        MPI_Reduce(fromBase, toBase, to - from,
                                   mpiDataType, mpiRedOp, rootTask, comm);
                }

#ifdef LOG_DOUBLE_VALUES
                if (toBase) {
                    assert(d->elemsize == 8);
                    for(uint64_t i = 0; i < elemCount; i++)
                        laik_log(1, "    after at %d: %f", from + i,
                                 ((double*)toBase)[i]);
                }
#endif

            }

            if (ss) {
                ss->reduceCount++;
                ss->reducedBytes += (to - from) * d->elemsize;
            }
        }
    }

    // use 2x <task count> phases to avoid deadlocks
    // - count phases X: 0..<count-1>
    //     - receive from <task X> if <task X> lower rank
    //     - send to <task X> if <task X> is higher rank
    // - count phases Y: 0..<count-1>
    //     - receive from <task count-Y> if it is higher rank
    //     - send to <task count-1-Y> if it is lower rank
    //
    // TODO: prepare communication schedule with sorted transitions actions!

    int count = g->size;
    for(int phase = 0; phase < 2*count; phase++) {
        int task = (phase < count) ? phase : (2*count-phase-1);
        bool sendToHigher   = (phase < count);
        bool recvFromLower  = (phase < count);
        bool sendToLower    = (phase >= count);
        bool recvFromHigher = (phase >= count);

        // receive
        for(int i=0; i < t->recvCount; i++) {
            struct recvTOp* op = &(t->recv[i]);
            if (task != op->fromTask) continue;
            if (recvFromLower  && (myid < task)) continue;
            if (recvFromHigher && (myid > task)) continue;

            if (laik_log_begin(1)) {
                laik_log_append("MPI Recv ");
                laik_log_Slice(dims, &(op->slc));
                laik_log_flush(" from T%d", op->fromTask);
            }

            assert(myid != op->fromTask);

            assert(op->mapNo < toList->count);
            Laik_Mapping* toMap = toList->map[op->mapNo];
            assert(toMap != 0);
            if (toMap->base == 0) {
                // space not yet allocated
                laik_allocateMap(toMap, ss);
                assert(toMap->base != 0);
            }

            MPI_Status s;
            uint64_t count;

            MPI_Datatype mpiDataType = getMPIDataType(d);

            // TODO:
            // - tag 1 may conflict with application
            // - check status

            if (dims == 1) {
                // we directly support 1d data layouts

                // from global to receiver-local indexes
                int64_t from = op->slc.from.i[0] - toMap->requiredSlice.from.i[0];
                int64_t to   = op->slc.to.i[0] - toMap->requiredSlice.from.i[0];
                count = to - from;

                laik_log(1, "  direct recv to local [%lld;%lld[, slc/map %d/%d, "
                         "elemsize %d, baseptr %p\n",
                         (long long int) from, (long long int) to,
                         op->sliceNo, op->mapNo,
                         d->elemsize, (void*) toMap->base);

                if (mpi_bug > 0) {
                    // intentional bug: ignore small amounts of data received
                    if (count < 1000) {
                        char dummy[8000];
                        MPI_Recv(dummy, count,
                                 mpiDataType, op->fromTask, 1, comm, &s);
                        continue;
                    }
                }

                MPI_Recv(toMap->base + from * d->elemsize, count,
                         mpiDataType, op->fromTask, 1, comm, &s);
            }
            else {
                // use temporary receive buffer and layout-specific unpack

                // the used layout must support unpacking
                assert(toMap->layout->unpack);

                Laik_Index idx = op->slc.from;
                count = 0;
                int recvCount, unpacked;
                while(1) {
                    MPI_Recv(packbuf, PACKBUFSIZE / d->elemsize,
                             mpiDataType, op->fromTask, 1, comm, &s);
                    MPI_Get_count(&s, mpiDataType, &recvCount);
                    unpacked = (toMap->layout->unpack)(toMap, &(op->slc), &idx,
                                                       packbuf,
                                                       recvCount * d->elemsize);
                    assert(recvCount == unpacked);
                    count += unpacked;
                    if (laik_index_isEqual(dims, &idx, &(op->slc.to))) break;
                }
                assert(count == laik_slice_size(dims, &(op->slc)));
            }


            if (ss) {
                ss->recvCount++;
                ss->receivedBytes += count * d->elemsize;
            }


        }

        // send
        for(int i=0; i < t->sendCount; i++) {
            struct sendTOp* op = &(t->send[i]);
            if (task != op->toTask) continue;
            if (sendToLower  && (myid < task)) continue;
            if (sendToHigher && (myid > task)) continue;

            if (laik_log_begin(1)) {
                laik_log_append("MPI Send ");
                laik_log_Slice(dims, &(op->slc));
                laik_log_flush(" to T%d", op->toTask);
            }

            assert(myid != op->toTask);

            assert(op->mapNo < fromList->count);
            Laik_Mapping* fromMap = fromList->map[op->mapNo];
            // data to send must exist in local memory
            assert(fromMap);
            if (!fromMap->base) {
                laik_log_begin(LAIK_LL_Panic);
                laik_log_append("About to send data ('%s', slice ", d->name);
                laik_log_Slice(dims, &(op->slc));
                laik_log_flush(") to preserve it for the next phase as"
                                " requested by you, but it never was written"
                                " to in the previous phase. Fix your code!");
                assert(0);
            }

            uint64_t count;
            MPI_Datatype mpiDataType = getMPIDataType(d);

            if (dims == 1) {
                // we directly support 1d data layouts

                // from global to sender-local indexes
                int64_t from = op->slc.from.i[0] - fromMap->requiredSlice.from.i[0];
                int64_t to   = op->slc.to.i[0] - fromMap->requiredSlice.from.i[0];
                count = to - from;

                laik_log(1, "  direct send: from local [%lld;%lld[, slice/map %d/%d, "
                            "elemsize %d, baseptr %p\n",
                         (long long int) from, (long long int) to,
                         op->sliceNo, op->mapNo,
                         d->elemsize, (void*) fromMap->base);

                // TODO: tag 1 may conflict with application
                MPI_Send(fromMap->base + from * d->elemsize, count,
                         mpiDataType, op->toTask, 1, comm);
            }
            else {
                // use temporary receive buffer and layout-specific unpack

                // the used layout must support packing
                assert(fromMap->layout->pack);

                Laik_Index idx = op->slc.from;
                uint64_t size = laik_slice_size(dims, &(op->slc));
                assert(size > 0);
                int packed;
                count = 0;
                while(1) {
                    packed = (fromMap->layout->pack)(fromMap, &(op->slc), &idx,
                                                     packbuf, PACKBUFSIZE);
                    assert(packed > 0);
                    MPI_Send(packbuf, packed,
                             mpiDataType, op->toTask, 1, comm);
                    count += packed;
                    if (laik_index_isEqual(dims, &idx, &(op->slc.to))) break;
                }
                assert(count == size);
            }

            if (ss) {
                ss->sendCount++;
                ss->sentBytes += count * d->elemsize;
            }
        }

    }
    #endif
}
