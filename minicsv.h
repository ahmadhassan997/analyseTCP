// The MIT License (MIT)
// Minimalistic CSV Streams 1.4
// Copyright (C) 2014, by Wong Shao Voon (shaovoon@yahoo.com)
//
// http://opensource.org/licenses/MIT
//
// version 1.2 : make use of make_shared
// version 1.3 : fixed: to when reading the last line and it does not have linefeed
//               added: skip_1st_line and skip_line functions to ifstream class
// version 1.4 : Removed the use of smart ptr.
// version 1.5 : Performance increase on writing without flushing every line.

//#define USE_BOOST_LEXICAL_CAST

#ifndef MiniCSV_H
	#define MiniCSV_H

#include <string>
#include <sstream>
#include <fstream>

#ifdef USE_BOOST_LEXICAL_CAST
#	include <boost/lexical_cast.hpp>
#endif

#define NEWLINE '\n'

namespace csv
{

	class ifstream
	{
	public:
		ifstream() : str(""), pos(0), delimiter(',')
		{
		}
		ifstream(const char * file, std::ios_base::openmode mode)
		{
			open(file, mode);
		}
		void open(const char * file, std::ios_base::openmode mode)
		{
			init();
			istm.open(file, mode);
		}
		void init()
		{
			str = "";
			pos = 0;
			delimiter = ',';
		}
		void close()
		{
			istm.close();
		}
		bool is_open()
		{
			return istm.is_open();
		}
		// eof is replaced by read_line
		//bool eof() const
		void set_delimiter(char delimiter_)
		{
			delimiter = delimiter_;
		}
		char get_delimiter() const
		{
			return delimiter;
		}
		void skip_line()
		{
			if(!istm.eof())
			{
				std::getline(istm, str);
				pos = 0;
			}
		}
		bool read_line()
		{
			this->str = "";
			if(!istm.eof())
			{
				std::getline(istm, this->str);
				pos = 0;

				if(this->str.empty())
					return false;

				return true;
			}
			return false;
		}
		std::string get_delimited_str()
		{
			std::string dStr = "";
			char ch = '\0';
			do
			{
				if(pos >= this->str.size())
				{
					this->str = "";

					return dStr;
				}

				ch = this->str[pos];
				++(pos);
				if(ch == delimiter || ch == '\r' || ch == '\n')
					break;

				dStr += ch;
			}
			while(true);

			return dStr;
		}
	private:
		std::ifstream istm;
		std::string str;
		size_t pos;
		char delimiter;
	};

	class ofstream
	{
	public:

		ofstream() : after_newline(true), delimiter(',')
		{
		}
		ofstream(const std::string &file) : ofstream(file, std::ios_base::out) {}
		ofstream(const std::string &file, std::ios_base::openmode mode) :
			ofstream(file.c_str(), mode) {}

		ofstream(const char * file, std::ios_base::openmode mode)
		{
			open(file, mode);
		}
		~ofstream() {
			close();
		}

		void open(const char * file, std::ios_base::openmode mode)
		{
			init();
			ostm.open(file, mode);
			filename = std::string(file);
		}
		void init()
		{
			after_newline = true;
			delimiter = ',';
		}
		void flush()
		{
			ostm.flush();
		}
		void close()
		{
			ostm.close();
		}
		bool is_open()
		{
			return ostm.is_open();
		}
		void set_delimiter(char delimiter_)
		{
			delimiter = delimiter_;
		}
		char get_delimiter() const
		{
			return delimiter;
		}
		void set_after_newline(bool after_newline_)
		{
			after_newline = after_newline_;
		}
		bool get_after_newline() const
		{
			return after_newline;
		}
		std::ofstream& get_ofstream()
		{
			if (!ostm.is_open())
				throw std::ios_base::failure(string("File stream for file '") + filename + "' is closed!");
			return ostm;
		}
	private:
		std::ofstream ostm;
		std::string filename;
		bool after_newline;
		char delimiter;
	};


} // ns csv

template<typename T>
csv::ifstream& operator >> (csv::ifstream& istm, T& val)
{
	std::string str = istm.get_delimited_str();

#ifdef USE_BOOST_LEXICAL_CAST
	val = boost::lexical_cast<T>(str);
#else
	std::istringstream is(str);
	is >> val;
#endif

	return istm;
}
template<>
inline csv::ifstream& operator >> (csv::ifstream& istm, std::string& val)
{
	val = istm.get_delimited_str();

	return istm;
}

template<typename T>
csv::ofstream& operator << (csv::ofstream& ostm, const T& val)
{
	if(!ostm.get_after_newline())
		ostm.get_ofstream() << ostm.get_delimiter();

	ostm.get_ofstream() << val;

	ostm.set_after_newline(false);

	return ostm;
}
template<>
inline csv::ofstream& operator << (csv::ofstream& ostm, const char& val)
{
	if(val==NEWLINE)
	{
		ostm.get_ofstream() << NEWLINE;

		ostm.set_after_newline(true);
	}
	else
		ostm.get_ofstream() << val;

	return ostm;
}

#endif // MiniCSV_H
