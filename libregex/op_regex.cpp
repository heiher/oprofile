/**
 * @file op_regex.cpp
 * This file contains implementation for a lightweight wrapper around
 * libc regex, providing regular expression match and replace facility.
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 * @remark Idea comes from TextFilt project <http://textfilt.sourceforge.net>
 *
 * @author Philippe Elie
 */

#include <cerrno>

#include <iostream>
#include <fstream>

#include "string_manip.h"

#include "op_regex.h"

using namespace std;

namespace {

bad_regex::bad_regex(string const & pattern)
	: op_exception(pattern)
{
}


string op_regerror(int err, regex_t const & regexp)
{
	size_t needed_size = regerror(err, &regexp, 0, 0);
	char * buffer = new char [needed_size];
	regerror(err, &regexp, buffer, needed_size);

	return buffer;
}


void op_regcomp(regex_t & regexp, string const & pattern)
{
	int err = regcomp(&regexp, pattern.c_str(), REG_EXTENDED);
	if (err) {
		throw bad_regex("regcomp error: " + op_regerror(err, regexp)
				+ " for pattern : " + pattern);
	}
}


bool op_regexec(regex_t const & regex, char const * str, regmatch_t * match,
	       size_t nmatch)
{
	return regexec(&regex, str, nmatch, match, 0) != REG_NOMATCH;
}


void op_regfree(regex_t & regexp)
{
	regfree(&regexp);
}


// return the index number associated with a char seen in a "\x".
// Allowed range are for x is [0-9a-z] return size_t(-1) if x is not in
// these ranges.
size_t subexpr_index(char ch)
{
	if (!isdigit(ch) && !(ch >='a' && ch <= 'z'))
		return size_t(-1);

	return ch >= 'a' ? ch - 'a' + 10  : ch - '0';
}

}  // anonymous namespace


regular_expression_replace::regular_expression_replace(size_t limit_,
						       size_t limit_defs)
	:
	limit(limit_),
	limit_defs_expansion(limit_defs)
{
}


regular_expression_replace::~regular_expression_replace()
{
	for (size_t i = 0 ; i < v_regexp.size() ; ++i)
		op_regfree(v_regexp[i]);
}


void regular_expression_replace::add_definition(string const & name,
						string const & definition)
{
	string expanded_definition;
	expand_string(definition, expanded_definition);

	defs[name] = expanded_definition;
}


void regular_expression_replace::add_pattern(string const & pattern,
					     string const & replace)
{
	string expanded_pattern;
	expand_string(pattern, expanded_pattern);

	regex_t regexp;
	op_regcomp(regexp, expanded_pattern);
	v_regexp.push_back(regexp);
	v_replace.push_back(replace);
}


void regular_expression_replace::expand_string(string const & input,
					       string & result)
{
	string last, expanded(input);
	size_t i = 0;
	for (i = 0 ; i < limit_defs_expansion ; ++i) {
		last = expanded;
		expanded = substitute_definition(last);
		if (expanded == last) {
			break;
		}
	}

	if (i == limit_defs_expansion) {
		throw bad_regex("too many substitution for: + input");
	}

	result = last;
}


string regular_expression_replace::substitute_definition(string const & pattern)
{
	string result;
	bool previous_is_escape = false;

	for (size_t i = 0 ; i < pattern.length() ; ++i) {
		if (pattern[i] == '$' && !previous_is_escape) {
			size_t pos = pattern.find('{', i);
			if (pos != i + 1) {
				throw bad_regex("invalid $ in pattern: " + pattern);
			}
			size_t end = pattern.find('}', i);
			if (end == string::npos) {
				throw bad_regex("no matching '}' in pattern: " + pattern);
			}
			string def_name = pattern.substr(pos+1, (end-pos) - 1);
			if (defs.find(def_name) == defs.end()) {
				throw bad_regex("definition not found and used in pattern: (" + def_name + ") " + pattern);
			}
			result += defs[def_name];
			i = end;
		} else {
			if (pattern[i] == '\\' && !previous_is_escape) {
				previous_is_escape = true;
			} else {
				previous_is_escape = false;
			}
			result += pattern[i];
		}
	}

	return result;
}


// FIXME limit output string size ? (cause we can have exponential growing
// of output string through a rule "a" = "aa")
bool regular_expression_replace::execute(string & str) const
{
	bool changed = true;
	for (size_t nr_iter = 0; changed && nr_iter < limit ; ++nr_iter) {
		changed = false;
		for (size_t i = 0 ; i < v_regexp.size() ; ++i) {
			if (do_execute(str, v_regexp[i], v_replace[i])) {
				changed = true;
			}
		}
	}

	// this don't return if the input string has been changed but if
	// we reach the limit number of iteration.
	return changed == false;
}


bool regular_expression_replace::do_execute(string & str,
					    regex_t const & regexp,
					    string const & replace) const
{
	bool changed = false;

	regmatch_t match[max_match];
	size_t last_pos = 0;
	for (size_t nr_iter = 0;
	     op_regexec(regexp, str.c_str() + last_pos, match, max_match) &&
	       nr_iter < limit;
	     nr_iter++) {
		changed = true;
		do_replace(str, last_pos, replace, match);
	}

	return changed;
}


void regular_expression_replace::do_replace(string & str, size_t start_pos,
					    string const & replace,
					    regmatch_t const * match) const
{
	string inserted;
	for (size_t i = 0 ; i < replace.length() ; ++i) {
		if (replace[i] == '\\') {
			if (i == replace.length() - 1) {
				throw bad_regex("illegal \\ trailer: " + replace);
			}
			++i;
			if (replace[i] == '\\') {
				inserted += '\\';
			}  else if (subexpr_index(replace[i]) != size_t(-1)) {
				size_t sub_expr = subexpr_index(replace[i]);
				if (sub_expr >= max_match) {
					throw bad_regex("illegal group index :" + replace);
				} else if (match[sub_expr].rm_so == -1 &&
					   match[sub_expr].rm_eo == -1) {
					// empty match: nothing todo
				} else if (match[sub_expr].rm_so == -1 ||
					   match[sub_expr].rm_eo == -1) {
					throw bad_regex("illegal match: " + replace);
				} else {
					inserted += str.substr(match[sub_expr].rm_so, match[sub_expr].rm_eo - match[sub_expr].rm_so);
				}
			} else {
				throw bad_regex("expect group index :" + replace);
			}
		} else {
			inserted += replace[i];
		}
	}

	size_t first = match[0].rm_so + start_pos;
	size_t count = match[0].rm_eo - match[0].rm_so;

	str.replace(first, count, inserted);
}


void setup_regex(regular_expression_replace & regex,
                 string const & filename)
{
	ifstream in(filename.c_str());
	if (!in) {
		throw op_fatal_error("Can't open file " + filename +
				     " for reading", errno);
	}

	regular_expression_replace var_name_rule;
	var_name_rule.add_pattern("^\\$([_a-zA-Z][_a-zA-Z0-9]*)[ ]*=.*", "\\1");
	regular_expression_replace var_value_rule;
	var_value_rule.add_pattern(".*=[ ]*\"(.*)\"", "\\1");

	regular_expression_replace left_rule;
	left_rule.add_pattern("[ ]*\"(.*)\"[ ]*=.*", "\\1");
	regular_expression_replace right_rule;
	right_rule.add_pattern(".*=[ ]*\"(.*)\"", "\\1");

	string line;
	while (getline(in, line)) {
		line = trim(line);
		if (line.empty() || line[0] == '#')
			continue;

		string temp = line;
		var_name_rule.execute(temp);
		if (temp == line) {
			string left = line;
			left_rule.execute(left);
			if (left == line) {
				throw bad_regex("invalid input file: " +
						'"' + line + '"');
			}

			string right = line;
			right_rule.execute(right);
			if (right == line) {
				throw bad_regex("invalid input file: "
						+ '"' + line + '"');
			}

			regex.add_pattern(left, right);
		} else {
			// temp != line ==> var_name_rule succeed to substitute
			// into temp the var_name present in line
			string var_name = temp;
			string var_value = line;
			var_value_rule.execute(var_value);
			if (var_value == line) {
				throw bad_regex("invalid input file: " +
						'"' + line + '"');
			}

			regex.add_definition(var_name, var_value);
		}
	}
}
