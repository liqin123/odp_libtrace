#include "libtrace.h"
#include "libtrace_int.h"
#include "data-struct/vector.h"
#include <assert.h>
#include <stdlib.h>

static int init_combiner(libtrace_t *t, libtrace_combine_t *c) {
	int i = 0;
	assert(trace_get_perpkt_threads(t) > 0);
	libtrace_vector_t *queues;
	c->queues = calloc(sizeof(libtrace_vector_t), trace_get_perpkt_threads(t));
	queues = c->queues;
	for (i = 0; i < trace_get_perpkt_threads(t); ++i) {
		libtrace_vector_init(&queues[i], sizeof(libtrace_result_t));
	}
	return 0;
}

static void publish(libtrace_t *trace UNUSED, int t_id, libtrace_combine_t *c, libtrace_result_t *res) {
	libtrace_vector_t *vec = &((libtrace_vector_t*)c->queues)[t_id];
	libtrace_vector_push_back(vec, res);
}

static void read(libtrace_t *trace UNUSED, libtrace_combine_t *c UNUSED){
	return;
}

static int compare_result(const void* p1, const void* p2)
{
	const libtrace_result_t * r1 = p1;
	const libtrace_result_t * r2 = p2;
	if (r1->key < r2->key)
		return -1;
	if (r1->key == r2->key)
		return 0;
	else
		return 1;
}

static void pause(libtrace_t *trace, libtrace_combine_t *c) {
	libtrace_vector_t *queues = c->queues;
	int i;
	for (i = 0; i < trace_get_perpkt_threads(trace); ++i) {
		libtrace_vector_apply_function(&queues[i], (vector_data_fn) libtrace_make_result_safe);
	}
}

static void read_final(libtrace_t *trace, libtrace_combine_t *c) {
	libtrace_vector_t *queues = c->queues;
	int i;
	size_t a;
	// Combine all results into queue 1
	for (i = 1; i < trace_get_perpkt_threads(trace); ++i)
	{
		libtrace_vector_append(&queues[0],&queues[i]);
	}
	// Sort them
	libtrace_vector_qsort(&queues[0], compare_result);

	for (a = 0; a < libtrace_vector_get_size(&queues[0]); ++a) {
		libtrace_result_t r;
		libtrace_generic_t gt = {.res = &r};
		ASSERT_RET (libtrace_vector_get(&queues[0], a, (void *) &r), == 1);
		if (r.type == RESULT_TICK_INTERVAL ||
                                r.type == RESULT_TICK_COUNT) {
                        /* Ticks are essentially useless for this combiner? */
                        continue;
                }
                send_message(trace, &trace->reporter_thread, MESSAGE_RESULT,
                                gt, NULL);
	}
	libtrace_vector_empty(&queues[0]);
}

static void destroy(libtrace_t *trace, libtrace_combine_t *c) {
	int i;
	libtrace_vector_t *queues = c->queues;

	for (i = 0; i < trace_get_perpkt_threads(trace); i++) {
		assert(libtrace_vector_get_size(&queues[i]) == 0);
		libtrace_vector_destroy(&queues[i]);
	}
	free(queues);
	queues = NULL;
}

DLLEXPORT const libtrace_combine_t combiner_sorted = {
    init_combiner,	/* initialise */
	destroy,		/* destroy */
	publish,		/* publish */
    read,			/* read */
    read_final,			/* read_final */
    pause,			/* pause */
    NULL,			/* queues */
    0,                          /* last_count_tick */
    0,                          /* last_ts_tick */
    {0}				/* opts */
};
