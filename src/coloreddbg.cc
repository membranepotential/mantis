/*
 * ============================================================================
 *
 *       Filename:  main.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2016-11-10 03:31:54 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prashant Pandey (), ppandey@cs.stonybrook.edu
 *   Organization:  Stony Brook University
 *
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <utility>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <set>
#include <unordered_set>
#include <bitset>
#include <cassert>
#include <fstream>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <openssl/rand.h>
#include "MantisFS.h"
#include "sparsepp/spp.h"
#include "tsl/sparse_map.h"
#include "ProgOpts.h"
#include "coloreddbg.h"

#define MAX_THREADS 50
#define MAX_NUM_SAMPLES 2600
#define SAMPLE_SIZE (1ULL << 26)

struct thread_object {
	ColoredDbg<SampleObject<CQF<KeyObject>*>, KeyObject> *cdbg;
	SampleObject<CQF<KeyObject>*> *inobjects;
	std::unordered_map<std::string, uint64_t> cutoffs;
	cdbg_bv_map_t<BitVector, std::pair<uint64_t,uint64_t>, sdslhash<BitVector>>
		map;
	uint64_t start_hash;
	uint64_t end_hash;
	uint64_t num_kmers;
};

typedef struct thread_object thread_object;

void *thread_construct(void *object) {
	thread_object *obj = (thread_object*)object;
	obj->cdbg->construct(obj->inobjects, obj->cutoffs, obj->map,
											 obj->start_hash, obj->end_hash, obj->num_kmers);

	return NULL;
}

void perform_construction(thread_object args[], uint32_t nthreads) {
	pthread_t threads[nthreads];

	for (uint32_t i = 0; i < nthreads; i++) {
		std::cout << "Starting thread " << i << " from " << args[i].start_hash <<
 						 " to " << args[i].end_hash << std::endl;
		if (pthread_create(&threads[i], NULL, &thread_construct, &args[i])) {
			std::cerr << "Error creating thread " << i << std::endl;
			abort();
		}
	}

	for (uint32_t i = 0; i < nthreads; i++)
		if (pthread_join(threads[i], NULL)) {
			std::cerr << "Error joining thread " << i << std::endl;
			abort();
		}
}

/*
 * ===  FUNCTION  =============================================================
 *         Name:  main
 *  Description:  
 * ============================================================================
 */
	int
build_main ( BuildOpts& opt )
{
	/* calling asyc read init */
	struct aioinit aioinit;
	memset(&aioinit, 0, sizeof(struct aioinit));
	aioinit.aio_num = 2500;
	aioinit.aio_threads = 512;
	aio_init(&aioinit);

	std::ifstream infile(opt.inlist);
	std::ifstream cutofffile(opt.cutoffs);
	//struct timeval start1, end1;
	//struct timezone tzp;
	SampleObject<CQF<KeyObject>*> *inobjects;
	CQF<KeyObject> *cqfs;


	// Allocate QF structs for input CQFs
	inobjects = new SampleObject<CQF<KeyObject>*>[MAX_NUM_SAMPLES];
	cqfs = (CQF<KeyObject>*)calloc(MAX_NUM_SAMPLES, sizeof(CQF<KeyObject>));

	// Read cutoffs files
	std::unordered_map<std::string, uint64_t> cutoffs;
	std::string sample_id;
	uint32_t cutoff;
	while (cutofffile >> sample_id >> cutoff) {
		std::pair<std::string, uint32_t> pair(last_part(sample_id, '/'), cutoff);
		cutoffs.insert(pair);
	}

	// mmap all the input cqfs
	std::string cqf_file;
	uint32_t nqf = 0;
	while (infile >> cqf_file) {
		cqfs[nqf] = CQF<KeyObject>(cqf_file, true);
		std::string sample_id = first_part(first_part(last_part(cqf_file, '/'),
																									'.'), '_');
		std::cout << "Reading CQF " << nqf << " Seed " << cqfs[nqf].seed() <<
			std::endl;
		std::cout << "Sample id " << sample_id << " cut off " <<
							 cutoffs.find(sample_id)->second << std::endl;
		cqfs[nqf].dump_metadata();
		inobjects[nqf] = SampleObject<CQF<KeyObject>*>(&cqfs[nqf], sample_id, nqf);
		nqf++;
	}

	std::string prefix(opt.out);
  if (prefix.back() != '/') {
    prefix += '/';
  }
  // make the output directory if it doesn't exist
  if (!mantis::fs::DirExists(prefix.c_str())) {
    mantis::fs::MakeDir(prefix.c_str());
  }

	ColoredDbg<SampleObject<CQF<KeyObject>*>, KeyObject> cdbg(inobjects[0].obj->keybits(),
																														inobjects[0].obj->seed(),
																														nqf);

	cdbg.build_sampleid_map(inobjects);

	std::cout << "Sampling eq classes based on " << SAMPLE_SIZE << " kmers." <<
		std::endl;
	// First construct the colored dbg on 1000 k-mers.
	cdbg_bv_map_t<BitVector, std::pair<uint64_t,uint64_t>,
		sdslhash<BitVector>> unsorted_map;

	unsorted_map = cdbg.construct(inobjects, cutoffs, unsorted_map, 0,
																UINT64_MAX, SAMPLE_SIZE);

	DEBUG_CDBG("Number of eq classes found " << unsorted_map.size());

	// Sort equivalence classes based on their abundances.
	std::multimap<uint64_t, BitVector, std::greater<uint64_t>> sorted;
	for (auto& it : unsorted_map) {
		//DEBUG_CDBG(it.second.second << " " << it.second.first << " " << it.first.data());
		sorted.insert(std::pair<uint64_t, BitVector>(it.second.second, it.first));
		//sorted[it.second.second] = it.first;
	}
	cdbg_bv_map_t<BitVector, std::pair<uint64_t,uint64_t>,
		sdslhash<BitVector>> sorted_map;
	//DEBUG_CDBG("After sorting.");
	uint64_t i = 1;
	for (auto& it : sorted) {
		//DEBUG_CDBG(it.first << " " << it.second.data());
		std::pair<uint64_t, uint64_t> val(i, 0);
		std::pair<BitVector, std::pair<uint64_t, uint64_t>> keyval(it.second, val);
		sorted_map.insert(keyval);
		i++;
	}

	std::cout << "Constructing the colored dBG." << std::endl;

	thread_object args[MAX_THREADS];
	for (int i = 0; i < opt.numthreads; i ++) {
		args[i].cdbg = &cdbg;
		args[i].inobjects = inobjects;
		args[i].cutoffs = cutoffs;
		args[i].map = sorted_map;
		args[i].start_hash = i * (cdbg.range() / opt.numthreads);
		args[i].end_hash = (i + 1) * (cdbg.range() / opt.numthreads);
		args[i].num_kmers = UINT64_MAX;
	}

	// Reconstruct the colored dbg using the new set of equivalence classes.
	perform_construction(args, opt.numthreads);
	//cdbg.construct(inobjects, cutoffs, sorted_map, 0, UINT64_MAX, UINT64_MAX);

	std::cout << "Final colored dBG has " << cdbg.get_cqf()->size() <<
		" k-mers and " << cdbg.get_num_eqclasses() << " equivalence classes."
		<< std::endl;

	//cdbg.get_cqf()->dump_metadata();
	//DEBUG_CDBG(cdbg.get_cqf()->set_size());

	std::cout << "Serializing CQF and eq classes in " << prefix << std::endl;
	cdbg.serialize(prefix);
	std::cout << "Serialization done." << std::endl;

	return EXIT_SUCCESS;
}				/* ----------  end of function main  ---------- */
