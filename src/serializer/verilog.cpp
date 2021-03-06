#include <netlistDB/serializer/verilog.h>
#include <netlistDB/hw_type/hw_int.h>
#include <netlistDB/bit_utils.h>

using namespace std;
using namespace netlistDB::hw_type;
using namespace netlistDB::bit_utils;

namespace netlistDB {
namespace serializer {

Verilog2001::Verilog2001(iSerializationIO & io,
		std::map<const std::string, const void*> reserved_names,
		bool delete_io_after):
		Serializer(io, delete_io_after), name_scope(true), indent_cnt(0) {
	name_scope.set_level(1);
	auto & scope = *name_scope[0];
	for (auto & kw : keywords) {
		scope[kw] = new KeyWord;
	}
	io.file_extension(DEFAULT_FILE_EXT);
}

Verilog2001::Verilog2001(std::ostream & str,
		std::map<const std::string, const void*> reserved_names) :
		Verilog2001(*new SerializeToStream(str), reserved_names, true) {
}

Verilog2001::Verilog2001(const std::string & root_dir,
		std::map<const std::string, const void*> reserved_names) :
		Verilog2001(*new SerializeToFiles(root_dir), reserved_names, true) {
}

void Verilog2001::serialize_comment(const std::string & comment) {
	if (comment.size()) {
		auto & str = io.str();
		indent() << "// ";
		for (auto ch: comment) {
			str << ch;
			if (ch == '\n')
				indent() << "// ";
		}
		str << endl;
	}
}

void Verilog2001::print_array_indexes(const hw_type::iHwType * t, bool first) {
	auto & str = io.str();
	auto at = dynamic_cast<const hw_type::iHwType_array*>(t);
	if (at) {
		if (first) {
			str << " ";
		}

		print_array_indexes(&at->elm_t, false);
		str << "[0:" << (at->size - 1) << "]";
	}
}

const hw_type::iHwType & Verilog2001::get_non_array_t(
		const hw_type::iHwType & _t) {
	const hw_type::iHwType * t = &_t;
	while (true) {
		auto at = dynamic_cast<const hw_type::iHwType_array*>(t);
		if (at) {
			t = &at->elm_t;
		} else {
			return *t;
		}
	}
	return *t;
}

void Verilog2001::serialize_net_def(const Net & n) {
	serialize_comment(n.__doc__);
	auto & str = io.str();
	indent();
	auto v_t = verilogTypeOfSig(n);
	if (v_t == VERILOG_NET_TYPE::VERILOG_REG) {
		str << "reg ";
	} else {
		str << "wire ";
	}
	if (serialize_type_usage(get_non_array_t(n.t)))
		str << " ";

	str << name_scope.checkedName(n.id.name, &n);
	print_array_indexes(&n.t, true);
	str << ";";
}

bool Verilog2001::serialize_type_usage(const hw_type::iHwType & t) {
	auto int_t = dynamic_cast<const HwInt*>(&t);
	if (int_t) {
		if (int_t->bit_length() == 1 and not int_t->has_to_be_vector) {
			// only single bit, does not require any type specification
			return false;
		} else {
			auto & str = io.str();
			if (int_t->is_signed)
				str << "signed ";
			str << "[" << int_t->bit_length() << "-1:0]";
			return true;
		}
	} else {
		throw runtime_error(
				std::string(__FILE__) + ":" + std::to_string(__LINE__)
						+ " not implemented for this type");
	}

}

void Verilog2001::serialize_value(
		const typename hw_type::HwInt::value_type & val) {
	auto & str = io.str();
	auto & t = val.t;
	str << t.bit_length();
	str << "'";

	constexpr size_t bits_per_nible = 4;
	size_t rounded_size = t.bit_length() / bits_per_nible;
	if (rounded_size * bits_per_nible < t.bit_length()) {
		rounded_size++;
	}
	auto str_flags = str.flags();
	size_t base = 2;

	if ((val.mask != 0 and val.mask != t.all_mask) or t.bit_length() == 1) {
		base = 2;
		if (t.bit_length() > 1) {
			// if all chars are full defined or full undefined
			bool first = true;
			bool can_be_in_hex = true;
			for (int B = rounded_size - 1; B >= 0; B--) {
				auto m = select_bits(val.mask, B * bits_per_nible,
						bits_per_nible);
				if (m != 0xf and m != 0x0) {
					if (first) {
						auto expected_m = select_bits(t.all_mask,
								B * bits_per_nible, bits_per_nible);
						if (m != expected_m) {
							can_be_in_hex = false;
							break;
						}
					} else {
						can_be_in_hex = false;
						break;
					}
				}
				first = false;
			}
			if (can_be_in_hex)
				base = 16;
		}
	} else {
		base = 16;
	}
	if (base == 2) {
		str << "b";
		for (int b = int(t.bit_length()) - 1; b >= 0; b--) {
			auto m = select_bits(val.mask, b, 1);
			auto v = select_bits(val.value, b, 1);
			if (m) {
				if (v) {
					str << "1";
				} else {
					str << "0";
				}
			} else {
				str << "X";
			}
		}
	} else if (base == 16) {
		// base = 16
		str << "h";

		for (int B = rounded_size - 1; B >= 0; B--) {
			auto m = select_bits(val.mask, B * bits_per_nible, bits_per_nible);
			auto v = select_bits(val.value, B * bits_per_nible, bits_per_nible);
			if (m) {
				str << hex << int(v);
			} else {
				str << "X";
			}
		}
	} else {
		throw std::runtime_error("not supported base for the number");
	}

	str.flags(str_flags);
}

void Verilog2001::serialize_net_usage(const Net & n) {
	if (n.id.hidden) {
		if (n.val) {
			assert(
					n.drivers.size() == 0
							&& "constant net should not have any other driver");
			serialize_value(*n.val);
		} else {
			assert(n.drivers.size() == 1);
			auto op = dynamic_cast<const FunctionCall*>(n.drivers[0]);
			assert(n.drivers.size() == 1);
			assert(op);
			serialize(*op);
		}
	} else {
		io.str() << name_scope.checkedName(n.id.name, &n);
	}
}

Verilog2001::~Verilog2001() {
	auto & scope = *name_scope[0];
	for (auto & kw : keywords) {
		delete reinterpret_cast<const KeyWord*>(scope[kw]);
	}
}

void Verilog2001::serialize_block(const vector<Statement*> & stms) {
	auto & str = io.str();
	if (stms.size() == 1) {
		str << endl;
		indent_cnt++;
		serialize_stm(*stms[0]);
		str << endl;
		indent_cnt--;
	} else {
		str << " begin" << std::endl;

		indent_cnt++;
		for (auto s : stms) {
			serialize_stm(*s);
			str << endl;
		}
		indent_cnt--;

		indent() << "end";
	}
}

std::ostream & Verilog2001::indent() {
	auto & str = io.str();
	for (size_t i = 0; i < indent_cnt; i++) {
		str << INDENT;
	}
	return str;
}

enum Verilog2001::VERILOG_NET_TYPE Verilog2001::verilogTypeOfSig(
		const Net & n) const {
	size_t driver_cnt = n.drivers.size();
	if (n.direction == Direction::DIR_IN) {
		// is input port
		return VERILOG_NET_TYPE::VERILOG_WIRE;
	} else if (driver_cnt == 1) {
		auto d = dynamic_cast<Assignment*>(n.drivers[0]);
		if (d and not d->parent and not d->dst_index.size()
				and not d->sens.now_is_event_dependent)
			// primitive assignment
			return VERILOG_NET_TYPE::VERILOG_WIRE;
	}

	return VERILOG_NET_TYPE::VERILOG_REG;
}

const std::map<const FunctionDef*, int> & Verilog2001::get_operator_precedence() {
	return opPrecedence;
}

}
}
