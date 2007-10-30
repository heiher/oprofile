/**
 * @file locate_images.h
 * Location of binary images
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef LOCATE_IMAGES_H
#define LOCATE_IMAGES_H

#include <string>
#include <map>
#include <vector>

#include "image_errors.h"

/**
 * A class containing mappings from an image basename,
 * such as 'floppy.ko', to locations in the paths passed
 * in to populate().
 *
 * The name may exist multiple times; all locations are recorded
 * in this container.
 */
class extra_images {
public:
	extra_images();

	/// add all filenames found in the given paths prefixed by the
	/// archive path, recursively
	void populate(std::vector<std::string> const & paths,
		      std::string const & archive_path);

	/// base class for matcher functors object
	struct matcher {
		std::string const & value;
	public:
		explicit matcher(std::string const & v) : value(v) {}
		virtual ~matcher() {}
		/// default functor allowing trivial match
		virtual bool operator()(std::string const & str) const {
			return str == value;
		}
	};

	/**
	 * return a vector of all directories that match the functor
	 */
	std::vector<std::string> const find(matcher const & match) const;

	/// return a vector of all directories that match the given name
	std::vector<std::string> const find(std::string const & name) const;

	/**
	 * @param image_name binary image name
	 * @param error errors are flagged in this passed enum ref
	 * @param fixup if true return the fixed image name else always return
	 *  image_name and update error
	 *
	 * Locate a (number of) matching absolute paths to the given image
	 * name. If we fail to find the file we fill in error and return the
	 * original string.
	 */
	std::string const find_image_path(std::string const & image_name,
				image_error & error, bool fixup) const;

	/// return the archive path used to populate the images name map
	std::string get_archive_path() const { return archive_path; }

	/// return the uid for this extra_images, first valid uid is 1
	int get_uid() const { return uid; }

private:
	typedef std::multimap<std::string, std::string> images_t;
	typedef images_t::value_type value_type;
	typedef images_t::const_iterator const_iterator;

	/// map from image basename to owning directory
	images_t images;
	/// the archive path passed to populate the images name map.
	std::string archive_path;

	/// unique identifier, first valid uid is 1
	int uid;
	/// unique uid generator
	static int suid;
};

#endif /* LOCATE_IMAGES_H */
