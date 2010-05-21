/**
 * Copyright (C) 2010 Johannes Weißl <jargon@molb.org>
 * License: your favourite BSD-style license
 *
 * See OptionParser.h for help.
 */

#include "OptionParser.h"

#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <complex>

#if ENABLE_NLS
# include <libintl.h>
# ifndef _
#  define _(s) gettext(s)
# endif
#else
# ifndef _
#  define _(s) s
# endif
#endif

using namespace std;

namespace optparse {

////////// auxiliary string functions { //////////
struct str_wrap {
  str_wrap(const string& l, const string& r) : lwrap(l), rwrap(r) {}
  str_wrap(const string& w) : lwrap(w), rwrap(w) {}
  string operator() (const string& s) { return lwrap + s + rwrap; }
  const string lwrap, rwrap;
};
template<typename InputIterator, typename UnaryOperator>
static string str_join_trans(const string& sep, InputIterator begin, InputIterator end, UnaryOperator op) {
  string buf;
  for (InputIterator it = begin; it != end; ++it) {
    if (it != begin)
      buf += sep;
    buf += op(*it);
  }
  return buf;
}
template<class InputIterator>
static string str_join(const string& sep, InputIterator begin, InputIterator end) {
  return str_join_trans(sep, begin, end, str_wrap(""));
}
static string& str_replace(string& s, const string& patt, const string& repl) {
  size_t pos = 0, n = patt.length();
  while (true) {
    pos = s.find(patt, pos);
    if (pos == string::npos)
      break;
    s.replace(pos, n, repl);
    pos += repl.size();
  }
  return s;
}
static string str_replace(const string& s, const string& patt, const string& repl) {
  string tmp = s;
  str_replace(tmp, patt, repl);
  return tmp;
}
static string str_format(const string& s, size_t pre, size_t len, bool indent_first) {
  stringstream ss;
  string p;
  if (indent_first)
    p = string(pre, ' ');

  size_t pos = 0, linestart = 0;
  size_t line = 0;
  while (true) {
    bool wrap = false;

    size_t new_pos = s.find_first_of(" \n\t", pos);
    if (new_pos == string::npos)
      break;
    if (s[new_pos] == '\n') {
      pos = new_pos + 1;
      wrap = true;
    }
    if (line == 1)
      p = string(pre, ' ');
    if (wrap || new_pos + p.length() > linestart + len) {
      ss << p << s.substr(linestart, pos - linestart - 1) << endl;
      linestart = pos;
      line++;
    }
    pos = new_pos + 1;
  }
  ss << p << s.substr(linestart) << endl;
  return ss.str();
}
static string str_inc(const string& s) {
  stringstream ss;
  string v = (s != "") ? s : "0";
  long i;
  istringstream(v) >> i;
  ss << i+1;
  return ss.str();
}
////////// } auxiliary string functions //////////


////////// class OptionParser { //////////
OptionParser::OptionParser() :
  _usage(_("%prog [options]")),
  _add_help_option(true),
  _add_version_option(true) {
}

Option& OptionParser::add_option(const string& opt) {
  const string tmp[1] = { opt };
  return add_option(vector<string>(&tmp[0], &tmp[1]));
}
Option& OptionParser::add_option(const string& opt1, const string& opt2) {
  const string tmp[2] = { opt1, opt2 };
  return add_option(vector<string>(&tmp[0], &tmp[2]));
}
Option& OptionParser::add_option(const string& opt1, const string& opt2, const string& opt3) {
  const string tmp[3] = { opt1, opt2, opt3 };
  return add_option(vector<string>(&tmp[0], &tmp[3]));
}
Option& OptionParser::add_option(const vector<string>& v) {
  _opts.resize(_opts.size()+1);
  Option& option = _opts.back();
  string dest_fallback;
  for (vector<string>::const_iterator it = v.begin(); it != v.end(); ++it) {
    if (it->substr(0,2) == "--") {
      const string s = it->substr(2);
      if (option.dest() == "")
        option.dest(str_replace(s, "-", "_"));
      option._long_opts.insert(s);
      _optmap_l[s] = &option;
    } else {
      const string s = it->substr(1,1);
      if (dest_fallback == "")
        dest_fallback = s;
      option._short_opts.insert(s);
      _optmap_s[s] = &option;
    }
  }
  if (option.dest() == "")
    option.dest(dest_fallback);
  return option;
}

const Option& OptionParser::lookup_short_opt(const string& opt) const {
  optMap::const_iterator it = _optmap_s.find(opt);
  if (it == _optmap_s.end())
    error(_("no such option") + string(": -") + opt);
  return *it->second;
}

void OptionParser::handle_short_opt(const string& opt, const string& arg) {

  _remaining.pop_front();
  string value;

  const Option& option = lookup_short_opt(opt);
  if (option._nargs == 1) {
    value = arg.substr(2);
    if (value == "") {
      if (_remaining.empty())
        error("-" + opt + " " + _("option requires an argument"));
      value = _remaining.front();
      _remaining.pop_front();
    }
  } else {
    if (arg.length() > 2)
      _remaining.push_front(string("-") + arg.substr(2));
  }

  process_opt(option, string("-") + opt, value);
}

const Option& OptionParser::lookup_long_opt(const string& opt) const {

  list<string> matching;
  for (optMap::const_iterator it = _optmap_l.begin(); it != _optmap_l.end(); ++it) {
    if (it->first.compare(0, opt.length(), opt) == 0)
      matching.push_back(it->first);
  }
  if (matching.size() > 1) {
    string x = str_join(", ", matching.begin(), matching.end());
    error(_("ambiguous option") + string(": --") + opt + " (" + x + "?)");
  }
  if (matching.size() == 0)
    error(_("no such option") + string(": --") + opt);

  return *_optmap_l.find(matching.front())->second;
}

void OptionParser::handle_long_opt(const string& optstr) {

  _remaining.pop_front();
  string opt, value;

  size_t delim = optstr.find("=");
  if (delim != string::npos) {
    opt = optstr.substr(0, delim);
    value = optstr.substr(delim+1);
  } else
    opt = optstr;

  const Option& option = lookup_long_opt(opt);
  if (option._nargs == 1 and delim == string::npos) {
    if (not _remaining.empty()) {
      value = _remaining.front();
      _remaining.pop_front();
    }
  }

  if (option._nargs == 1 and value == "")
    error("--" + opt + " " + _("option requires an argument"));

  process_opt(option, string("--") + opt, value);
}

Values& OptionParser::parse_args(const int argc, char const* const* const argv) {
  if (_prog == "")
    _prog = argv[0];
  return parse_args(&argv[1], &argv[argc]);
}
Values& OptionParser::parse_args(const vector<string>& args) {

  _remaining.assign(args.begin(), args.end());

  if (add_help_option())
    add_option("-h", "--help") .action("help") .help(_("show this help message and exit"));
  if (add_version_option() and version() != "")
    add_option("--version") .action("version") .help(_("show program's version number and exit"));

  while (not _remaining.empty()) {
    const string arg = _remaining.front();

    if (arg == "--") {
      _remaining.pop_front();
      break;
    }

    if (arg.substr(0,2) == "--") {
      handle_long_opt(arg.substr(2));
    } else if (arg.substr(0,1) == "-" and arg.length() > 1) {
      handle_short_opt(arg.substr(1,1), arg);
    } else {
      _remaining.pop_front();
      _leftover.push_back(arg);
    }
  }
  while (not _remaining.empty()) {
    const string arg = _remaining.front();
    _remaining.pop_front();
    _leftover.push_back(arg);
  }

  for (strMap::const_iterator it = _defaults.begin(); it != _defaults.end(); ++it) {
    if (not _values.is_set(it->first))
      _values[it->first] = it->second;
  }

  for (list<Option>::const_iterator it = _opts.begin(); it != _opts.end(); ++it) {
    if (it->get_default() != "" and not _values.is_set(it->dest()))
        _values[it->dest()] = it->get_default();
  }

  return _values;
}

void OptionParser::process_opt(const Option& o, const string& opt, const string& value) {
  if (o.action() == "store") {
    string err = o.check_type(opt, value);
    if (err != "")
      error(err);
    _values[o.dest()] = value;
  } else
  if (o.action() == "store_const") {
    _values[o.dest()] = o.get_const();
  } else
  if (o.action() == "store_true") {
    _values[o.dest()] = "1";
  } else
  if (o.action() == "store_false") {
    _values[o.dest()] = "0";
  } else
  if (o.action() == "count") {
    _values[o.dest()] = str_inc(_values[o.dest()]);
  } else
  if (o.action() == "help") {
    print_help();
    std::exit(0);
  } else
  if (o.action() == "version") {
    print_version();
    std::exit(0);
  }
}

string OptionParser::format_option_help() const {
  stringstream ss;

  if (_opts.empty())
    return ss.str();

  ss << _("Options") << ":" << endl;
  for (list<Option>::const_iterator it = _opts.begin(); it != _opts.end(); ++it) {
    ss << it->format_help();
  }

  return ss.str();
}

string OptionParser::format_help() const {
  stringstream ss;

  if (usage() != "")
    ss << get_usage() << endl;

  if (description() != "")
    ss << str_format(description(), 0, 80, true) << endl;

  ss << format_option_help();

  if (epilog() != "")
    ss << endl << str_format(epilog(), 0, 80, true);

  return ss.str();
}
void OptionParser::print_help() const {
  cout << format_help();
}

void OptionParser::set_usage(const string& u) {
  string lower = u;
  transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  if (lower.compare(0, 7, "usage: ") == 0)
    _usage = u.substr(7);
  else
    _usage = u;
}
string OptionParser::format_usage(const string& u) const {
  stringstream ss;
  ss << _("Usage") << ": " << u << endl;
  return ss.str();
}
string OptionParser::get_usage() const {
  return format_usage(str_replace(usage(), "%prog", prog()));
}
void OptionParser::print_usage(ostream& out) const {
  out << get_usage() << endl;
}
void OptionParser::print_usage() const {
  print_usage(cout);
}

string OptionParser::get_version() const {
  return str_replace(_version, "%prog", prog());
}
void OptionParser::print_version(std::ostream& out) const {
  out << get_version() << endl;
}
void OptionParser::print_version() const {
  print_version(cout);
}

void OptionParser::exit() const {
  std::exit(2);
}
void OptionParser::error(const string& msg) const {
  print_usage(cerr);
  cerr << prog() << ": " << _("error") << ": " << msg << endl;
  exit();
}
////////// } class OptionParser //////////

////////// class Values { //////////
const string& Values::operator[] (const string& d) const {
  strMap::const_iterator it = _map.find(d);
  static const string& _empty = "";
  return (it != _map.end()) ? it->second : _empty;
}
bool Values::is_set(const string& d) const {
  return _map.find(d) != _map.end();
}
////////// } class Values //////////

////////// class Option { //////////
string Option::check_type(const string& opt, const string& val) const {
  istringstream ss(val);
  stringstream err;

  if (type() == "int" || type() == "long") {
    long t;
    if (not (ss >> t))
      err << _("option") << " " << opt << ": " << _("invalid integer value") << ": '" << val << "'";
  }
  else if (type() == "float" || type() == "double") {
    double t;
    if (not (ss >> t))
      err << _("option") << " " << opt << ": " << _("invalid floating-point value") << ": '" << val << "'";
  }
  else if (type() == "choice") {
    if (find(choices().begin(), choices().end(), val) == choices().end()) {
      list<string> tmp = choices();
      transform(tmp.begin(), tmp.end(), tmp.begin(), str_wrap("'"));
      err << _("option") << " " << opt << ": " << _("invalid choice") << ": '" << val << "'"
        << " (" << _("choose from") << " " << str_join(", ", tmp.begin(), tmp.end()) << ")";
    }
  }
  else if (type() == "complex") {
    complex<double> t;
    if (not (ss >> t))
      err << _("option") << " " << opt << ": " << _("invalid complex value") << ": '" << val << "'";
  }

  return err.str();
}

string Option::format_option_help() const {

  string mvar_short, mvar_long;
  if (nargs() == 1) {
    string mvar = metavar();
    if (mvar == "") {
      mvar = type();
      transform(mvar.begin(), mvar.end(), mvar.begin(), ::toupper);
    }
    mvar_short = " " + mvar;
    mvar_long = "=" + mvar;
  }

  stringstream ss;
  ss << "  ";

  if (not _short_opts.empty()) {
    ss << str_join_trans(", ", _short_opts.begin(), _short_opts.end(), str_wrap("-", mvar_short));
    if (not _long_opts.empty())
      ss << ", ";
  }
  if (not _long_opts.empty())
    ss << str_join_trans(", ", _long_opts.begin(), _long_opts.end(), str_wrap("--", mvar_long));

  return ss.str();
}

string Option::format_help() const {
  stringstream ss;
  string h = format_option_help();
  bool indent_first;
  ss << h;
  if (h.length() > 22) {
    ss << endl;
    indent_first = true;
  } else {
    ss << string(24 - h.length(), ' ');
    indent_first = false;
    if (help() == "")
      ss << endl;
  }
  if (help() != "")
    ss << str_format(help(), 24, 80, indent_first);
  return ss.str();
}

Option& Option::action(const string& a) {
  _action = a;
  if (a == "store_const" || a == "store_true" || a == "store_false" ||
      a == "append_const" || a == "count" || a == "help" || a == "version")
    nargs(0);
  return *this;
}
////////// } class Option //////////

}
