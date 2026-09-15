import random
import string
from typing import Tuple, Optional, List, Dict, Any
from dataclasses import dataclass, field

# ----------------------------------------------------------------------
# Helpers
# ----------------------------------------------------------------------

def indent(lines: List[str], level: int = 1) -> str:
    return "\n".join("    " * level + line if line.strip() else "" for line in lines)

def random_name(prefix: str = "x") -> str:
    return prefix + ''.join(random.choices(string.ascii_lowercase, k=4))

def random_int() -> int:
    return random.randint(-10, 10)   # small range to avoid overflow

def random_float() -> float:
    return round(random.uniform(-10.0, 10.0), 2)

def random_bool() -> bool:
    return random.choice([True, False])

def random_string() -> str:
    chars = random.choices(string.ascii_letters + string.digits, k=random.randint(0, 5))
    return ''.join(chars)

# ----------------------------------------------------------------------
# Feature selection
# ----------------------------------------------------------------------

class FeatureSet:
    def __init__(self):
        self.allow_async = True
        self.allow_decorators = True
        self.allow_type_annotations = True
        self.allow_defaults = True
        self.allow_variadic = True
        self.allow_overloads = True
        self.allow_lambdas = True
        self.allow_generators = True
        self.allow_recursion = True
        self.allow_named_args = True
        self.allow_spread = True
        self.allow_higher_order = True
        self.allow_method_calls = True
        self.allow_external = True

# ----------------------------------------------------------------------
# Random base
# ----------------------------------------------------------------------

class RandomGenerator:
    def __init__(self, seed: Optional[int] = None):
        if seed is not None:
            random.seed(seed)
        self.features = FeatureSet()

    def rand_choice(self, items):
        return random.choice(items)

    def rand_int(self, a: int = -10, b: int = 10) -> int:
        return random.randint(a, b)

    def rand_float(self, a: float = -10.0, b: float = 10.0) -> float:
        return round(random.uniform(a, b), 2)

    def rand_bool(self) -> bool:
        return random.choice([True, False])

    def rand_name(self, prefix: str = "v") -> str:
        return prefix + ''.join(random.choices(string.ascii_lowercase, k=4))

    def rand_string_literal(self) -> str:
        s = ''.join(random.choices(string.ascii_letters + string.digits, k=random.randint(0, 5)))
        return f'"{s}"'

    def rand_number_literal(self) -> str:
        return str(self.rand_int())

# ----------------------------------------------------------------------
# Decorator builder – separated by target
# ----------------------------------------------------------------------

class DecoratorBuilder(RandomGenerator):
    # NOTE: "log" is a standalone statement, not a function decorator
    FUNCTION_DECORATORS = ["profile", "track", "notrack"]   # removed "log"
    CLASS_DECORATORS = ["dataclass", "immutable", "singleton"]

    def build_decorators(self, target: str = "function") -> List[str]:
        if not self.features.allow_decorators:
            return []
        if target == "function":
            pool = self.FUNCTION_DECORATORS
        else:
            pool = self.CLASS_DECORATORS
        count = random.randint(0, min(2, len(pool)))
        return random.sample(pool, count)

# ----------------------------------------------------------------------
# Type builder
# ----------------------------------------------------------------------

class TypeBuilder(RandomGenerator):
    BUILTIN_TYPES = ["int", "float", "string", "bool", "list", "map", "tuple", "set", "frozen_set"]
    CUSTOM_TYPES = []

    def build_type(self, allow_custom: bool = True) -> Optional[str]:
        if not self.features.allow_type_annotations:
            return None
        if random.random() < 0.3:
            return None
        if allow_custom and self.CUSTOM_TYPES and random.random() < 0.5:
            return random.choice(self.CUSTOM_TYPES)
        return random.choice(self.BUILTIN_TYPES)

# ----------------------------------------------------------------------
# Expression builder – generates only type-safe expressions
# ----------------------------------------------------------------------

class ExpressionBuilder(RandomGenerator):
    def build_expression(self, depth: int = 0, var_names: List[str] = None,
                         avoid_var: Optional[str] = None) -> str:
        """Build an expression that doesn't reference 'avoid_var'."""
        if var_names is None:
            var_names = []
        # Filter out the variable being assigned
        safe_vars = [v for v in var_names if v != avoid_var] if avoid_var else var_names
        if depth > 2 or random.random() < 0.6:
            return self.build_literal_or_var(safe_vars)
        left = self.build_expression(depth + 1, safe_vars)
        op = random.choice(["+", "-", "*", "/", "%", "==", "!=", ">", "<", ">=", "<="])
        right = self.build_expression(depth + 1, safe_vars)
        # Avoid mixing bool with numbers in arithmetic/comparison
        # (we keep it simple – always generate numbers for arithmetic)
        return f"({left} {op} {right})"

    def build_literal_or_var(self, var_names: List[str]) -> str:
        if var_names and random.random() < 0.3:
            return random.choice(var_names)
        return self.rand_number_literal()

# ----------------------------------------------------------------------
# Statement builder (with depth control and loop tracking)
# ----------------------------------------------------------------------

class StatementBuilder(RandomGenerator):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.var_names = []          # variables defined in current scope
        self.inside_loop = False     # whether we are inside a loop
        self.max_depth = 2           # reduce nesting to avoid deep errors

    def build_statements(self, count: int = None, depth: int = 0) -> List[str]:
        if depth > self.max_depth:
            return []
        if count is None:
            count = random.randint(1, 2)   # fewer statements = fewer errors
        stmts = []
        for _ in range(count):
            stmts.append(self.build_statement(depth))
        return stmts

    def build_statement(self, depth: int = 0) -> str:
        if depth >= self.max_depth:
            choices = ["assign", "expr", "give"]
        else:
            choices = ["assign", "if", "for", "while", "loop", "break", "continue",
                       "give", "yield", "await", "try", "call", "expr"]
        # Only allow break/continue if inside a loop
        if not self.inside_loop:
            choices = [c for c in choices if c not in ("break", "continue")]
        kind = random.choice(choices)

        if kind == "assign":
            return self.build_assignment(depth)
        elif kind == "if":
            return self.build_if(depth)
        elif kind == "for":
            return self.build_for(depth)
        elif kind == "while":
            return self.build_while(depth)
        elif kind == "loop":
            return self.build_loop(depth)
        elif kind == "break":
            return self.build_break()
        elif kind == "continue":
            return self.build_continue()
        elif kind == "give":
            return self.build_give(depth)
        elif kind == "yield":
            return self.build_yield(depth)
        elif kind == "await":
            return self.build_await(depth)
        elif kind == "try":
            return self.build_try(depth)
        elif kind == "call":
            return self.build_call_stmt(depth)
        else:  # expr
            return self.build_expr(depth)

    def build_assignment(self, depth: int) -> str:
        var = self.rand_name("v")
        self.var_names.append(var)   # variable is now defined
        expr = ExpressionBuilder().build_expression(0, self.var_names, avoid_var=var)
        return f"{var} = {expr}"

    def build_if(self, depth: int) -> str:
        cond = ExpressionBuilder().build_expression(0, self.var_names)
        old_vars = self.var_names.copy()
        body = self.build_block(depth + 1)
        self.var_names = old_vars   # block scope reset
        stmt = f"if {cond} :\n{body}\n;"
        if random.random() < 0.3:
            else_body = self.build_block(depth + 1)
            self.var_names = old_vars
            stmt += f"\nelse :\n{else_body}\n;"
        return stmt

    def build_for(self, depth: int) -> str:
        loop_var = self.rand_name("i")
        iterable = random.choice([f"[{self.rand_number_literal()}, {self.rand_number_literal()}]",
                                  f"range({self.rand_int(1,4)})"])
        old_vars = self.var_names.copy()
        self.var_names.append(loop_var)  # loop variable is defined inside
        old_inside_loop = self.inside_loop
        self.inside_loop = True
        body = self.build_block(depth + 1)
        self.inside_loop = old_inside_loop
        self.var_names = old_vars   # reset scope
        return f"for {loop_var} in {iterable} :\n{body}\n;"

    def build_while(self, depth: int) -> str:
        cond = ExpressionBuilder().build_expression(0, self.var_names)
        old_vars = self.var_names.copy()
        old_inside_loop = self.inside_loop
        self.inside_loop = True
        body = self.build_block(depth + 1)
        self.inside_loop = old_inside_loop
        self.var_names = old_vars
        return f"while {cond} :\n{body}\n;"

    def build_loop(self, depth: int) -> str:
        safe = "@safe_loop " if random.random() < 0.2 else ""
        old_vars = self.var_names.copy()
        old_inside_loop = self.inside_loop
        self.inside_loop = True
        body = self.build_block(depth + 1)
        self.inside_loop = old_inside_loop
        self.var_names = old_vars
        return f"{safe}loop :\n{body}\n;"

    def build_break(self) -> str:
        if random.random() < 0.2:
            return "break " + self.rand_number_literal()
        return "break"

    def build_continue(self) -> str:
        return "continue"

    def build_give(self, depth: int) -> str:
        if random.random() < 0.4:
            return "give"
        expr = ExpressionBuilder().build_expression(0, self.var_names)
        return f"give {expr}"

    def build_yield(self, depth: int) -> str:
        if random.random() < 0.4:
            return "yield"
        expr = ExpressionBuilder().build_expression(0, self.var_names)
        return f"yield {expr}"

    def build_await(self, depth: int) -> str:
        expr = ExpressionBuilder().build_expression(0, self.var_names)
        return f"await {expr}"

    def build_try(self, depth: int) -> str:
        old_vars = self.var_names.copy()
        body = self.build_block(depth + 1)
        self.var_names = old_vars
        catch_body = self.build_block(depth + 1)
        self.var_names = old_vars
        return f"try :\n{body}\n;\ncatch e :\n{catch_body}\n;"

    def build_call_stmt(self, depth: int) -> str:
        fn_name = self.rand_name("fn")
        args = self.build_arg_list()
        return f"{fn_name}({','.join(args)})"

    def build_expr(self, depth: int) -> str:
        expr = ExpressionBuilder().build_expression(0, self.var_names)
        return expr   # expression statement – no semicolon

    def build_block(self, depth: int) -> str:
        stmts = self.build_statements(depth=depth)
        return indent(stmts)

    def build_arg_list(self) -> List[str]:
        count = random.randint(0, 2)
        args = []
        for _ in range(count):
            if random.random() < 0.3 and self.var_names:
                args.append(random.choice(self.var_names))
            else:
                args.append(self.rand_number_literal())
        return args

# ----------------------------------------------------------------------
# Function builder – no decorators on methods; adds a `give` at end
# ----------------------------------------------------------------------

@dataclass
class FunctionDef:
    name: str
    params: List[str]
    param_types: List[Optional[str]]
    defaults: List[Optional[str]]
    variadic: bool
    variadic_name: str
    return_type: Optional[str]
    is_async: bool
    decorators: List[str]
    body: List[str]
    is_generator: bool

class FunctionBuilder(RandomGenerator):
    def build_function(self, existing_names: List[str] = None, for_class: bool = False) -> FunctionDef:
        name = self.rand_name("f")
        param_count = random.randint(0, 2)   # fewer params = simpler
        params = [self.rand_name("p") for _ in range(param_count)]
        param_types = [TypeBuilder().build_type() for _ in params]
        defaults = [None] * param_count
        if self.features.allow_defaults:
            for i in range(param_count):
                if random.random() < 0.3:
                    defaults[i] = self.rand_number_literal()
        variadic = False
        variadic_name = ""
        if self.features.allow_variadic and random.random() < 0.1:
            variadic = True
            variadic_name = self.rand_name("rest")
        return_type = TypeBuilder().build_type() if self.features.allow_type_annotations else None
        is_async = self.features.allow_async and random.random() < 0.2

        # Decorators: only for top-level functions, not for class methods
        decorators = []
        if not for_class:
            decorators = DecoratorBuilder().build_decorators("function")

        stmt_builder = StatementBuilder()
        is_generator = False
        body_statements = []
        num_stmts = random.randint(1, 3)
        for _ in range(num_stmts):
            stmt = stmt_builder.build_statement(0)
            if "yield" in stmt:
                is_generator = True
            body_statements.append(stmt)

        # If not a generator, add a `give` at the end (unless body already has one)
        if not is_generator and not any("give" in s for s in body_statements):
            # Make sure we have a value to give
            expr = ExpressionBuilder().build_expression(0, stmt_builder.var_names)
            body_statements.append(f"give {expr}")

        return FunctionDef(
            name=name,
            params=params,
            param_types=param_types,
            defaults=defaults,
            variadic=variadic,
            variadic_name=variadic_name,
            return_type=return_type,
            is_async=is_async,
            decorators=decorators,
            body=body_statements,
            is_generator=is_generator
        )

    def function_to_source(self, fdef: FunctionDef) -> str:
        lines = []
        for dec in fdef.decorators:
            lines.append(f"@{dec}")
        async_prefix = "async " if fdef.is_async else ""
        params = []
        for i, p in enumerate(fdef.params):
            param_str = p
            if fdef.param_types[i] is not None:
                param_str += f": {fdef.param_types[i]}"
            if fdef.defaults[i] is not None:
                param_str += f" = {fdef.defaults[i]}"
            params.append(param_str)
        if fdef.variadic:
            params.append(f"...{fdef.variadic_name}")
        params_str = ", ".join(params)
        ret_str = f" -> {fdef.return_type}" if fdef.return_type else ""
        lines.append(f"{async_prefix}fn {fdef.name}({params_str}){ret_str} :")
        lines.extend(indent(fdef.body, 1).splitlines())
        lines.append(";")
        return "\n".join(lines)

# ----------------------------------------------------------------------
# Class builder – no decorators on methods
# ----------------------------------------------------------------------

class ClassBuilder(RandomGenerator):
    def build_class(self) -> str:
        name = self.rand_name("C")
        fields = []
        for _ in range(random.randint(0, 2)):
            fields.append(f"{self.rand_name('f')} = {self.rand_number_literal()}")
        # Build a method without decorators (for_class=True)
        fn = FunctionBuilder().build_function(for_class=True)
        fn.name = self.rand_name("method")
        fn.params.insert(0, "self")
        fn.param_types.insert(0, None)
        fn.defaults.insert(0, None)
        method_src = FunctionBuilder().function_to_source(fn)
        lines = [f"class {name} :"]
        if fields:
            lines.extend(indent(fields, 1).splitlines())
        lines.extend(indent([method_src], 1).splitlines())
        lines.append(";")
        decorators = DecoratorBuilder().build_decorators("class")
        if decorators:
            return "\n".join(f"@{d}" for d in decorators) + "\n" + "\n".join(lines)
        return "\n".join(lines)

# ----------------------------------------------------------------------
# Main generator – produces a program with top-level functions and main
# ----------------------------------------------------------------------

class ProgramGenerator(RandomGenerator):
    def generate(self) -> Tuple[str, str, str, int]:
        functions = []
        for _ in range(random.randint(1, 2)):
            functions.append(FunctionBuilder().build_function())

        class_src = ClassBuilder().build_class() if random.random() < 0.3 else ""

        # Build main statements
        main_stmts = []
        for f in functions:
            args = self.build_args_for_function(f)
            call = f"{f.name}({', '.join(args)})"
            if random.random() < 0.5:
                var = self.rand_name("r")
                main_stmts.append(f"{var} = {call}")
            else:
                main_stmts.append(call)

        # Add a few simple statements
        stmt_builder = StatementBuilder()
        for _ in range(random.randint(0, 1)):
            main_stmts.append(stmt_builder.build_statement(0))
        main_stmts.append('print("done")')

        source_lines = []
        if class_src:
            source_lines.append(class_src)
        for f in functions:
            source_lines.append(FunctionBuilder().function_to_source(f))
        source_lines.append("")
        source_lines.extend(main_stmts)
        source = "\n".join(source_lines)

        expected_stdout = "done\n"
        expected_stderr = ""
        expected_exit = 0
        return source, expected_stdout, expected_stderr, expected_exit

    def build_args_for_function(self, fdef: FunctionDef) -> List[str]:
        args = []
        for i, p in enumerate(fdef.params):
            if fdef.param_types[i] is not None:
                typ = fdef.param_types[i]
                if typ == "int":
                    args.append(str(self.rand_int()))
                elif typ == "float":
                    args.append(str(self.rand_float()))
                elif typ == "bool":
                    args.append("true" if self.rand_bool() else "false")
                elif typ == "string":
                    args.append(self.rand_string_literal())
                else:
                    args.append(self.rand_number_literal())
            else:
                args.append(self.rand_number_literal())
        if fdef.variadic:
            extra = [self.rand_number_literal() for _ in range(random.randint(0, 1))]
            args.extend(extra)
        return args

# ----------------------------------------------------------------------
# SemanticGenerator (wrapper for compatibility)
# ----------------------------------------------------------------------

class SemanticGenerator:
    def __init__(self, seed: Optional[int] = None):
        self.prog_gen = ProgramGenerator(seed=seed)

    def generate(self) -> Tuple[str, str, str, int]:
        return self.prog_gen.generate()