#include "crab/cfg.hpp"
#include "crab/types.hpp"

namespace crab {

class variable_ref_t {
  public:
    using bitwidth_t = typename variable_t::bitwidth_t;

  private:
    std::shared_ptr<variable_t> m_v{};

  public:
    variable_ref_t() {}

    variable_ref_t(variable_t v) : m_v(std::make_shared<variable_t>(v)) {}

    bool is_null() const { return !m_v; }

    variable_t get() const {
        assert(!is_null());
        return *m_v;
    }

    bool is_typed() const {
        assert(!is_null());
        return m_v->is_typed();
    }

    bool is_array_type() const {
        assert(!is_null());
        return m_v->is_array_type();
    }

    variable_type_t get_type() const {
        assert(!is_null());
        return m_v->get_type();
    }

    bool has_bitwidth() const {
        assert(!is_null());
        return m_v->has_bitwidth();
    }

    bitwidth_t get_bitwidth() const {
        assert(!is_null());
        return m_v->get_bitwidth();
    }

    const varname_t& name() const {
        assert(!is_null());
        return m_v->name();
    }

    varname_t& name() {
        assert(!is_null());
        return m_v->name();
    }

    index_t index() const {
        assert(!is_null());
        return m_v->index();
    }

    std::size_t hash() const {
        assert(!is_null());
        return m_v->hash();
    }

    void write(crab_os& o) const { return m_v->write(o); }
}; // class variable_ref_t

inline crab_os& operator<<(crab_os& o, const variable_ref_t& v) {
    v.write(o);
    return o;
}

void cfg_t::remove_useless_blocks() {
    if (!has_exit())
        return;

    cfg_rev_t rev_cfg(*this);

    visited_t useful, useless;
    mark_alive_blocks(rev_cfg.entry(), rev_cfg, useful);

    for (auto const& bb : *this) {
        if (!(useful.count(bb.label()) > 0)) {
            useless.insert(bb.label());
        }
    }

    for (auto bb_id : useless) {
        remove(bb_id);
    }
}

struct type_checker_visitor {

    type_checker_visitor() {}

    void check_num(variable_t v, std::string msg, new_statement_t s) {
        if (v.get_type() != INT_TYPE) {
            crab_string_os os;
            os << "(type checking) " << msg << " in " << s;
            CRAB_ERROR(os.str());
        }
    }

    void check_int(variable_t v, std::string msg, new_statement_t s) {
        if ((v.get_type() != INT_TYPE) || (v.get_bitwidth() <= 1)) {
            crab_string_os os;
            os << "(type checking) " << msg << " in " << s;
            CRAB_ERROR(os.str());
        }
    }

    void check_bitwidth_if_int(variable_t v, std::string msg, const new_statement_t s) {
        if (v.get_type() == INT_TYPE) {
            if (v.get_bitwidth() <= 1) {
                crab_string_os os;
                os << "(type checking) " << msg << " in " << s;
                CRAB_ERROR(os.str());
            }
        }
    }

    void check_same_type(variable_t v1, variable_t v2, std::string msg, new_statement_t s) {
        if (v1.get_type() != v2.get_type()) {
            crab_string_os os;
            os << "(type checking) " << msg << " in " << s;
            CRAB_ERROR(os.str());
        }
    }

    void check_same_bitwidth(variable_t v1, variable_t v2, std::string msg, new_statement_t s) {
        // assume v1 and v2 have same type
        if (v1.get_type() == INT_TYPE) {
            if (v1.get_bitwidth() != v2.get_bitwidth()) {
                crab_string_os os;
                os << "(type checking) " << msg << " in " << s;
                CRAB_ERROR(os.str());
            }
        }
    }

    void check_num_or_var(linear_expression_t e, std::string msg, new_statement_t s) {
        if (!(e.is_constant() || e.get_variable())) {
            crab_string_os os;
            os << "(type checking) " << msg << " in " << s;
            CRAB_ERROR(os.str());
        }
    }

    void check_array(variable_t v, new_statement_t s) {
        switch (v.get_type()) {
        case ARR_INT_TYPE: break;
        default: {
            crab_string_os os;
            os << "(type checking) " << v << " must be an array variable in " << s;
            CRAB_ERROR(os.str());
        }
        }
    }

    // v1 is array type and v2 is a scalar type consistent with v1
    void check_array_and_scalar_type(variable_t v1, variable_t v2, new_statement_t s) {
        switch (v1.get_type()) {
        case ARR_INT_TYPE:
            if (v2.get_type() == INT_TYPE)
                return;
            break;
        default: {
            crab_string_os os;
            os << "(type checking) " << v1 << " must be an array variable in " << s;
            CRAB_ERROR(os.str());
        }
        }
        crab_string_os os;
        os << "(type checking) " << v1 << " and " << v2 << " do not have consistent types in " << s;
        CRAB_ERROR(os.str());
    }

    void operator()(const binary_op_t& s) {
        variable_t lhs = s.lhs;
        linear_expression_t op1 = s.left;
        linear_expression_t op2 = s.right;

        check_num(lhs, "lhs must be integer or real", s);
        check_bitwidth_if_int(lhs, "lhs must be have bitwidth > 1", s);

        if (std::optional<variable_t> v1 = op1.get_variable()) {
            check_same_type(lhs, *v1, "first operand cannot have different type from lhs", s);
            check_same_bitwidth(lhs, *v1, "first operand cannot have different bitwidth from lhs", s);
        } else {
            CRAB_ERROR("(type checking) first binary operand must be a variable in ", s);
        }
        if (std::optional<variable_t> v2 = op2.get_variable()) {
            check_same_type(lhs, *v2, "second operand cannot have different type from lhs", s);
            check_same_bitwidth(lhs, *v2, "second operand cannot have different bitwidth from lhs", s);
        } else {
            // TODO: we can still check that we use z_number of INT_TYPE
        }
    }

    void operator()(const assign_t& s) {
        variable_t lhs = s.lhs;
        linear_expression_t rhs = s.rhs;

        check_num(lhs, "lhs must be integer or real", s);
        check_bitwidth_if_int(lhs, "lhs must be have bitwidth > 1", s);

        typename linear_expression_t::variable_set_t vars = rhs.variables();
        for (auto const& v : vars) {
            check_same_type(lhs, v, "variable cannot have different type from lhs", s);
            check_same_bitwidth(lhs, v, "variable cannot have different bitwidth from lhs", s);
        }
    }

    void operator()(const assume_t& s) {
        typename linear_expression_t::variable_set_t vars = s.constraint.variables();
        bool first = true;
        variable_ref_t first_var;
        for (auto const& v : vars) {
            check_num(v, "assume variables must be integer or real", s);
            if (first) {
                first_var = variable_ref_t(v);
                first = false;
            }
            check_same_type(first_var.get(), v, "inconsistent types in assume variables", s);
            check_same_bitwidth(first_var.get(), v, "inconsistent bitwidths in assume variables", s);
        }
    }

    void operator()(const assert_t& s) {
        typename linear_expression_t::variable_set_t vars = s.constraint.variables();
        bool first = true;
        variable_ref_t first_var;
        for (auto const& v : vars) {
            check_num(v, "assert variables must be integer or real", s);
            if (first) {
                first_var = variable_ref_t(v);
                first = false;
            }
            check_same_type(first_var.get(), v, "inconsistent types in assert variables", s);
            check_same_bitwidth(first_var.get(), v, "inconsistent bitwidths in assert variables", s);
        }
    }

    void operator()(const select_t& s) {
        check_num(s.lhs, "lhs must be integer or real", s);
        check_bitwidth_if_int(s.lhs, "lhs must be have bitwidth > 1", s);

        typename linear_expression_t::variable_set_t left_vars = s.left.variables();
        for (auto const& v : left_vars) {
            check_same_type(s.lhs, v, "inconsistent types in select variables", s);
            check_same_bitwidth(s.lhs, v, "inconsistent bitwidths in select variables", s);
        }
        typename linear_expression_t::variable_set_t right_vars = s.right.variables();
        for (auto const& v : right_vars) {
            check_same_type(s.lhs, v, "inconsistent types in select variables", s);
            check_same_bitwidth(s.lhs, v, "inconsistent bitwidths in select variables", s);
        }

        // -- The condition can have different bitwidth from
        //    lhs/left/right operands but must have same type.
        typename linear_expression_t::variable_set_t cond_vars = s.cond.variables();
        bool first = true;
        variable_ref_t first_var;
        for (auto const& v : cond_vars) {
            check_num(v, "assume variables must be integer or real", s);
            if (first) {
                first_var = variable_ref_t(v);
                first = false;
            }
            check_same_type(s.lhs, v, "inconsistent types in select condition variables", s);
            check_same_type(first_var.get(), v, "inconsistent types in select condition variables", s);
            check_same_bitwidth(first_var.get(), v, "inconsistent bitwidths in select condition variables", s);
        }
    }

    void operator()(const havoc_t&) {}

    void operator()(const array_init_t& s) {
        // TODO: check that e_sz is the same number that v's bitwidth
        variable_t a = s.array;
        linear_expression_t e_sz = s.elem_size;
        linear_expression_t lb = s.lb_index;
        linear_expression_t ub = s.ub_index;
        linear_expression_t v = s.val;
        check_array(a, s);
        check_num_or_var(e_sz, "element size must be number or variable", s);
        check_num_or_var(lb, "array lower bound must be number or variable", s);
        check_num_or_var(ub, "array upper bound must be number or variable", s);
        check_num_or_var(v, "array value must be number or variable", s);
        if (std::optional<variable_t> vv = v.get_variable()) {
            check_array_and_scalar_type(a, *vv, s);
        }
    }

    void operator()(const array_store_t& s) {
        // TODO: check that e_sz is the same number that v's bitwidth
        /// XXX: we allow linear expressions as indexes
        variable_t a = s.array;
        linear_expression_t e_sz = s.elem_size;
        linear_expression_t v = s.value;
        if (s.is_singleton) {
            if (!(s.lb_index.equal(s.ub_index))) {
                crab_string_os os;
                os << "(type checking) "
                    << "lower and upper indexes must be equal because array is a singleton in " << s;
                CRAB_ERROR(os.str());
            }
        }
        check_array(a, s);
        check_num_or_var(e_sz, "element size must be number or variable", s);
        check_num_or_var(v, "array value must be number or variable", s);
        if (std::optional<variable_t> vv = v.get_variable()) {
            check_array_and_scalar_type(a, *vv, s);
        }
    }

    void operator()(const array_load_t& s) {
        // TODO: check that e_sz is the same number that lhs's bitwidth
        /// XXX: we allow linear expressions as indexes
        variable_t a = s.array;
        linear_expression_t e_sz = s.elem_size;
        variable_t lhs = s.lhs;
        check_array(a, s);
        check_num_or_var(e_sz, "element size must be number or variable", s);
        check_array_and_scalar_type(a, lhs, s);
    }

    void operator()(const std::monostate& s) {
    }
}; // end class type_checker_visitor

void type_check(const cfg_ref_t& cfg) {
    type_checker_visitor vis;
    for (auto& bb : cfg) {
        for (const new_statement_t& statement : bb)
            std::visit(vis, statement);
    }
}

} // namespace crab