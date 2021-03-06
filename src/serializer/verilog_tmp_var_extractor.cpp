#include <netlistDB/serializer/verilog_tmp_var_extractor.h>
#include <netlistDB/operator_defs.h>
#include <netlistDB/hw_type/common.h>

namespace netlistDB {
namespace serializer {

VerilogTmpVarExtractor::VerilogTmpVarExtractor() :
		replacement_ctx("VerilogTmpExtractor_replacement_ctx") {
	unused = &replacement_ctx.sig("__unused__", hw_type::hw_bit);
}

void VerilogTmpVarExtractor::clear() {
	for (auto n : replacement_ctx.nodes) {
		delete n;
	}
	replacement_ctx.nodes.clear();
	replacement_ctx.nets.clear();
	replacements.clear();
	unused = &replacement_ctx.sig("__unused__", hw_type::hw_bit);
}

const Net & VerilogTmpVarExtractor::checked(const Net & n) {
	auto f = replacements.find(&n);
	if (f == replacements.end()) {
		return n;
	} else {
		return *f->second;
	}
}

bool VerilogTmpVarExtractor::is_unused(const Net & n) {
	return &checked(n) == unused;
}

bool VerilogTmpVarExtractor::operand_is_another_operand(const Net & operand) {
	return (operand.val == nullptr and operand.id.hidden
			and operand.drivers.size() == 0
			and dynamic_cast<const FunctionCall*>(operand.drivers[0]));
}

void VerilogTmpVarExtractor::extract_as_tmp_var(const Net & n,
		const std::string & name_prefix) {
	throw std::runtime_error(
			std::string("Not implemented ") + std::string(__FILE__) + ":"
					+ std::to_string(__LINE__));
	assert(replacements.find(&n) == replacements.end());
	Net & tmpVar = replacement_ctx.sig(name_prefix, n.t);
	replacements[&n] = &tmpVar;
}

void VerilogTmpVarExtractor::visit(const FunctionCall & c) {
	if (&c.fn == &OpRising or &c.fn == &OpFalling) {
		replacements[&c.res] = unused;
		return;
	}

	// if operand is concatenation and parent operator
	// is not concatenation operand should be extracted
	// as tmp variable
	for (auto operand : c.args) {
		if (&c.fn != &OpConcat and operand_is_another_operand(*operand)
				and operand->drivers.size() == 1) {
			auto d = dynamic_cast<const FunctionCall*>(operand->drivers[0]);
			if (d and &d->fn == &OpConcat) {
				extract_as_tmp_var(*operand, "tmp_concat_");
				continue;
			}
		}
		visit(*operand);
	}
}

}
}
