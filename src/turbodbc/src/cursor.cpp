#include <turbodbc/cursor.h>
#include <turbodbc/make_description.h>
#include <turbodbc/result_sets/bound_result_set.h>
#include <turbodbc/r_types.h>
#include <turbodbc/shared_ptr.h>

#include <cpp_odbc/statement.h>
#include <cpp_odbc/error.h>

#include <boost/variant/get.hpp>
#include <sqlext.h>
#include <stdexcept>

#include <cstring>
#include <sstream>
#include <boost/make_shared.hpp>


namespace turbodbc {

cursor::cursor(std::shared_ptr<turbodbc::connection const> connection,
               turbodbc::buffer_size buffer_size,
               std::size_t parameter_sets_to_buffer,
               bool use_async_io) :
  connection_(connection),
	buffer_size_(buffer_size),
	parameter_sets_to_buffer_(parameter_sets_to_buffer),
	use_async_io_(use_async_io),
	query_()
{
	connection_->set_active_cursor(this);
}
cursor::cursor(boost::shared_ptr<turbodbc::connection const> connection,
               turbodbc::buffer_size buffer_size,
               std::size_t parameter_sets_to_buffer,
               bool use_async_io) :
	buffer_size_(buffer_size),
	parameter_sets_to_buffer_(parameter_sets_to_buffer),
	use_async_io_(use_async_io),
	query_()
{
	connection_ = make_shared_ptr<turbodbc::connection const>(connection);
	connection_->set_active_cursor(this);
}

cursor::~cursor() = default;

void cursor::prepare(std::string const & sql)
{
	results_.reset();
	query_.reset();
	auto statement = connection_->get_connection()->make_statement();
	statement->prepare(sql);
	query_ = boost::make_shared<query>(statement, buffer_size_, parameter_sets_to_buffer_, use_async_io_);
}

void cursor::execute()
{
	query_->execute();
	auto raw_result_set = query_->get_results();
	if (raw_result_set) {
		results_ = boost::make_shared<result_sets::r_result_set>(*raw_result_set);
	}
}

boost::shared_ptr<result_sets::r_result_set> cursor::get_result_set() const
{
	return results_;
}

void cursor::add_parameter_set(Rcpp::DataFrame const & df) {
  // determine types for each column
	std::vector<r_type> types;
	auto ncols = df.size();
	auto nrows = df.nrows();
	for (int col = 0; col < ncols; ++col) {
		types.push_back(get_r_type(df[col]));
	}

	std::vector<nullable_field> out(ncols);
	for (int row = 0; row < nrows; ++row) {
		for (int col = 0; col < df.size(); ++col) {
			//Rcpp::Rcout << "row: " << i << " col: " << j << " type: " << Rf_type2char(TYPEOF(df[j])) << '\n';
			out[col] = nullable_field_from_R_object(df[col], types[col], row);
		}
		add_parameter_set(out);
	}
}

// Need to convert the R type to standard types
void cursor::add_parameter_set(Rcpp::List const & parameter_set) {
	std::vector<nullable_field> out;
	out.reserve(Rf_length(parameter_set));
	for (int i = 0; i < Rf_length(parameter_set); ++i) {
		Rcpp::RObject x = parameter_set[i];
		out.push_back(nullable_field_from_R_object(x));
	}
	add_parameter_set(out);
}

void cursor::add_parameter_set(std::vector<nullable_field> const & parameter_set)
{
	query_->add_parameter_set(parameter_set);
}

long cursor::get_row_count()
{
	return query_->get_row_count();
}

std::shared_ptr<turbodbc::connection const> cursor::get_connection() const
{
	return connection_;
}

boost::shared_ptr<turbodbc::query> cursor::get_query()
{
	return query_;
}

Rcpp::List cursor::sql_tables(std::string const & catalog_name, std::string const & schema_name, std::string const & table_name, std::string const & table_type)
{
	auto st = connection_->get_connection()->make_statement();
	auto statement = make_shared_ptr<cpp_odbc::statement const>(st);
	statement->sql_tables(catalog_name, schema_name, table_name, table_type);

	auto raw_result_set = boost::make_shared<result_sets::bound_result_set>(statement, buffer_size_);
	if (raw_result_set) {
		auto result = boost::make_shared<result_sets::r_result_set>(*raw_result_set);
		return result->fetch();
	}
	return Rcpp::List();
}

Rcpp::List cursor::sql_columns(std::string const & catalog_name, std::string const & schema_name, std::string const & table_name, std::string const & table_type)
{
	auto st = connection_->get_connection()->make_statement();
	auto statement = make_shared_ptr<cpp_odbc::statement const>(st);
	statement->sql_columns(catalog_name, schema_name, table_name, table_type);

	auto raw_result_set = boost::make_shared<result_sets::bound_result_set>(statement, buffer_size_);
	if (raw_result_set) {
		auto result = boost::make_shared<result_sets::r_result_set>(*raw_result_set);
		return result->fetch();
	}
	return Rcpp::List();
}

bool cursor::is_active() const
{
	return connection_->is_active_cursor(this);
}

}
