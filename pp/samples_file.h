/**
 * @file samples_file.h
 * Encapsulation of samples files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef SAMPLES_FILE_H
#define SAMPLES_FILE_H

#include <string>

#include "db.h"
#include "op_types.h"

class counter_array_t;

/** A class to store one samples file */
struct samples_file_t
{
	/**
	 * samples_file_t - construct a samples_file_t object
	 * @param filename the full path of sample file
	 *
	 * open and mmap the samples file specified by filename
	 * samples file header coherence are checked
	 *
	 * all error are fatal
	 */
	samples_file_t(std::string const & filename);
 
	/**
	 * ~samples_file_t - destroy a samples_file_t object
	 *
	 * close and unmap the samples file
	 */
	~samples_file_t();

	/**
	 * check_headers - check than the lhs and rhs headers are
	 * coherent (same size, same mtime etc.)
	 * @param rhs the other samples_file_t
	 *
	 * all error are fatal
	 */
	bool check_headers(samples_file_t const & headers) const;

	/// return the sample count at the given position
	u32 count(uint start) const { 
		return count(start, start + 1);
	}

	/**
	 * count - return the number of samples in given range
	 * @param start start samples nr of range
	 * @param end end samples nr of range
	 *
	 * return the number of samples in the the range [start, end)
	 * no range checking is performed.
	 *
	 * This actually code duplicate partially accumulate member of
	 * opp_samples_files which in future must use this as it internal
	 * implementation
	 */
	u32 count(uint start, uint end) const;

	// FIXME: return struct opd_header const & ??
	/// return the header of this sample file
	struct opd_header const * header() const {
		return static_cast<opd_header *>(db_tree.base_memory);
	}

	// probably needs to be private and create the neccessary member
	// function (not simple getter), make private and compile to see
	// what operation we need later. I've currently not a clear view
	// of what we need
//private:
	db_tree_t db_tree;

	/**
	 * For the kernel and kernel modules, this value is non-zero and
	 * equal to the offset of the .text section. This is done because
	 * we use the information provided in /proc/ksyms, which only gives
	 * the mapped position of .text, and the symbol kernel_start from
	 * System.map. This value is used to fix up the sample offsets
	 * for kernel code as a result of this difference (in user-space
	 * samples, the sample offset is from the start of the mapped
	 * file, as seen in /proc/pid/maps).
	 */
	u32 sect_offset;

private:
	// neither copy-able or copy constructible
	// FIXME: let's derive from a non_copyable like in boost
	samples_file_t(samples_file_t const &);
	samples_file_t & operator=(samples_file_t const &);
};

// FIXME: can we split this file in two again ? I might want a samples_file_t
// w/o needing this storage. File naming is a pain though...
 
/** Store multiple samples files belonging to the same image and the same
 * session can hold OP_MAX_COUNTERS samples files */
struct opp_samples_files {
	/**
	 * opp_samples_files - construct an opp_samples_files object
	 * @param sample_file the base name of sample file
	 * @param counter which samples files to open, -1 means try to open
	 * all samples files.
	 *
	 * at least one sample file (based on sample_file name)
	 * must be opened. If more than one sample file is open
	 * their header must be coherent. Each header is also
	 * sanitized.
	 *
	 * all error are fatal
	 */
	opp_samples_files(std::string const & sample_file, int counter);

	/**
	 * ~opp_samples_files - destroy an object opp_samples
	 *
	 * close and free all related resource to the samples file(s)
	 */
	~opp_samples_files();

	/**
	 * is_open - test if a samples file is open
	 * @param i index of the samples file to check.
	 *
	 * return true if the samples file index is open
	 */ 
	bool is_open(int i) const {
		return samples[i] != 0;
	}
 
	/**
	 * @param i index of the samples files
	 * @param sample_nr number of the samples to test.
	 *
	 * return the number of samples for samples file index at position
	 * sample_nr. return 0 if the samples file is close or there is no
	 * samples at position sample_nr
	 */
	uint samples_count(int i, int sample_nr) const {
		return is_open(i) ? samples[i]->count(sample_nr) : 0;
	}

	/**
	 * accumulate_samples - lookup samples from a vma address
	 * @param counter where to accumulate the samples
	 * @param index index of the samples.
	 *
	 * return false if no samples has been found
	 */
	bool accumulate_samples(counter_array_t & counter, uint vma) const;
 
	/**
	 * accumulate_samples - lookup samples from a range of vma address
	 * @param counter where to accumulate the samples
	 * @param start start index of the samples.
	 * @param end end index of the samples.
	 *
	 * return false if no samples has been found
	 */
	bool accumulate_samples(counter_array_t & counter,
				uint start, uint end) const;

	// this look like a free fun
	// FIXME: so what's it doing in oprofpp_util.cpp ??? 
	void output_header() const;

	/// return the header of the first opened samples file
	struct opd_header const * first_header() const {
		return samples[first_file]->header();
	}

	void set_sect_offset(u32 sect_offset);

	// TODO privatize when we can
	samples_file_t * samples[OP_MAX_COUNTERS];
	uint nr_counters;
	std::string sample_filename;

	// used in do_list_xxxx/do_dump_gprof.
	size_t counter_mask;

private:
	// cached value: index to the first opened file, setup as nearly as we
	// can in ctor.
	int first_file;

	/**
	 * open_samples_file - ctor helper
	 * @param counter the counter number
	 * @param can_fail allow to fail gracefully
	 *
	 * open and mmap the given samples files,
	 * the member var samples[counter], header[counter]
	 * etc. are updated in case of success.
	 * The header is checked but coherence between
	 * header can not be sanitized at this point.
	 *
	 * if can_fail == false all error are fatal.
	 */
	void open_samples_file(u32 counter, bool can_fail);
};

#endif /* !SAMPLES_FILE_H */