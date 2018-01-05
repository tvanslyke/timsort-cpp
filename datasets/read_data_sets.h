#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <algorithm>
#include <sstream>
#include <utility>
#include <tuple>
#include <string>


static const std::string census_fname = "us_state_naics_detailedsizes_2015.txt";
static const std::string mnist_fname  = "train-labels-idx1-ubyte";

std::vector<int> read_mnist_data(const std::string& path)
{
	std::ifstream mnist_label_input(path, std::ios::binary);
	assert(mnist_label_input);
	mnist_label_input.ignore(8);
	std::istream_iterator<unsigned char> in_iter(mnist_label_input);
	std::istream_iterator<unsigned char> end;
	std::vector<int> v(54000);
	std::copy(in_iter, end, v.begin());
	return v;
}



std::size_t simple_csv_read_num_entry(std::istream& strm)
{
	std::string line("");
	std::size_t numentry{0};
	std::getline(strm, line, ',');
	std::stringstream(line) >> numentry;
	return numentry;
}
std::string simple_csv_read_string_entry(std::istream& strm)
{
	std::string entry;
	std::getline(strm, entry, ',');
	std::size_t quotepos = entry.find('\"');
	if(quotepos != std::string::npos)
	{
		entry = entry.substr(quotepos + 1, entry.size());
		std::string tail;
		std::getline(strm, tail, ',');
		quotepos = tail.find('\"');
		while(quotepos == std::string::npos)
		{
			entry += tail;
			std::getline(strm, tail, ',');
			quotepos = tail.find('\"');
		}
		entry += tail.substr(0, quotepos);
	}
	return entry;
}


using string_type = std::string;

using naics_t_ = std::tuple<std::size_t,  // STATE, 
			   string_type,  // NAICS,
			   std::size_t,  // ENTRSIZE,
			   std::size_t,  // FIRM,
			   std::size_t,  // ESTB,

			   std::size_t,  // EMPL_N,
			   string_type,  // EMPLFL_R,
			   string_type,  // EMPLFL_N,
			   std::size_t,  // PAYR_N,
			   string_type,  // PAYRFL_N,

			   string_type,  // STATEDSCR,
			   string_type,  // NAICSDSCR, 
			   string_type>; // entrsizedscr


// easier to read when debugging
struct naics_t: public naics_t_
{
	using naics_t_::naics_t_; 
};



void get_naics_entry(const std::string & line, naics_t& dest)
{
	std::stringstream linestrm(line);
	std::get<0>(dest) = simple_csv_read_num_entry(linestrm);
	std::get<1>(dest) = simple_csv_read_string_entry(linestrm);
	std::get<2>(dest) = simple_csv_read_num_entry(linestrm);
	std::get<3>(dest) = simple_csv_read_num_entry(linestrm);
	std::get<4>(dest) = simple_csv_read_num_entry(linestrm);

	std::get<5>(dest) = simple_csv_read_num_entry(linestrm);
	std::get<6>(dest) = simple_csv_read_string_entry(linestrm);
	std::get<7>(dest) = simple_csv_read_string_entry(linestrm);
	std::get<8>(dest) = simple_csv_read_num_entry(linestrm);
	std::get<9>(dest) = simple_csv_read_string_entry(linestrm);

	std::get<10>(dest) = simple_csv_read_string_entry(linestrm);
	std::get<11>(dest) = simple_csv_read_string_entry(linestrm);
	std::get<12>(dest) = simple_csv_read_string_entry(linestrm);
}

template <class DestIt>
void us_state_naics_detailedsizes_2015(const std::string& path, DestIt dest)
{
	std::ifstream data_in(path);
	assert(data_in);
	data_in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	std::string line;
	naics_t entry;
	while(data_in)
	{
		std::getline(data_in, line);
		get_naics_entry(line, entry);
		*dest++ = entry;
	}
}

std::vector<naics_t> read_census_data(const std::string& path)
{
	std::vector<naics_t> data;
	data.reserve(74998);
	us_state_naics_detailedsizes_2015(path, std::back_inserter(data));
	return data;
}
