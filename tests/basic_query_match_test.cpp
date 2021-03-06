
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE netlistDB_basic_query_test

#include <boost/test/unit_test.hpp>
#include <functional>
#include <iostream>
#include <fstream>
#include <stdint.h>

#include <netlistDB/netlist.h>
#include <netlistDB/statement_if.h>
#include <netlistDB/query/query_match.h>
#include <netlistDB/query/query_path.h>
#include <netlistDB/hw_type/common.h>


using namespace netlistDB;
using namespace netlistDB::query;
using namespace netlistDB::hw_type;


BOOST_AUTO_TEST_SUITE( netlistDB_testsuite )

BOOST_AUTO_TEST_CASE( query_result_of_add ) {
	Netlist ctx("adder");
	std::vector<std::tuple<Net *, Net *, Net *>> expected;
	size_t n = 20;

	for (size_t i = 0; i < n; i++) {
		auto &a = ctx.sig_in("a", hw_int32);
		auto &b = ctx.sig_in("b", hw_int32);
		auto &res = a + b;
		expected.push_back( { &a, &b, &res });

		// add some garbage
		auto & c = ctx.sig_in("c", hw_int32);
		~a;
		res & c;

		auto path = QueryPath::find_path(a, res);
		BOOST_CHECK_EQUAL(path.second, true);
		BOOST_CHECK_EQUAL(path.first.size(), 3);
		if (path.first.size() == 3) {
			std::vector<iNode*> ref = { &a, res.drivers[0], &res };
			size_t i = 0;
			for (auto p : path.first) {
				BOOST_CHECK_EQUAL(p, ref[i]);
				i++;
			}
		}
	}

	QueryMatch query_add;
	auto &r = query_add.sig_in("a", hw_int32) + query_add.sig_in("b", hw_int32);
	r.direction = Direction::DIR_OUT;

	auto qres = query_add.search(ctx);
	BOOST_CHECK_EQUAL(qres.size(), n);
}

BOOST_AUTO_TEST_CASE( simple_mux ) {
	Netlist ctx("mux");
	size_t N = 1;
	for (size_t i = 0; i < N; i++) {
		auto &a = ctx.sig_in("a", hw_bit);
		auto &b = ctx.sig_in("b", hw_bit);
		auto &c = ctx.sig_in("c", hw_bit);

		If(a)( { &a(b), }).Else( { &a(c), });
	}

	QueryMatch q_mux;
	auto &qa = q_mux.sig_in("a", hw_bit);
	auto &qb = q_mux.sig_in("b", hw_bit);
	auto &qc = q_mux.sig_in("c", hw_bit);

	If(qa)( { &qa(qb), }).Else( { &qa(qc), });

	auto qres = q_mux.search(q_mux);
	BOOST_CHECK_EQUAL(qres.size(), 1);
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()
