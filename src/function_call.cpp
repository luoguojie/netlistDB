#include <netlistDB/netlist.h>

namespace netlistDB {

FunctionCall::FunctionCall(FunctionDef & fn, Net & arg0, Net & res) :
		fn(fn), args( { &arg0 }), res(res) {
	arg0.ctx.register_node(*this);
	arg0.endpoints.push_back(this);
	res.drivers.push_back(this);
}

FunctionCall::FunctionCall(FunctionDef & fn, Net & arg0, Net & arg1, Net & res) :
		fn(fn), args( { &arg0, &arg1 }), res(res) {
	arg0.ctx.register_node(*this);
	arg0.endpoints.push_back(this);
	arg1.endpoints.push_back(this);
	res.drivers.push_back(this);
}

void FunctionCall::forward(const predicate_t & fn) {
	fn(res);
}

void FunctionCall::backward(const predicate_t & fn) {
	for (auto a : args) {
		if (fn(*a))
			return;
	}
}

}
