# Rules to Follow

Code should be generalized and multiply functional not specific. Don't
hard code calculations as final values - show the calculation in
compile time constant expressions with variable names or comments or
both.

In C and C++ I want to use camelCase instead of snake_case everywhere
and the "m_" prefix for instance members is not to be used. I want
2-space indentation rules, one space after each C++ keyword like if,
for, switch, while, etc. before the following expression in
parentheses. I want to use locally declared variables in for loops
instead of declaring these outside the for loop where possible. I want
generic index variables to be int. I want to use C string variables
and arrays instead of std::string or std::string_view where possible
to avoid all the verbosity and conversions to and from C string
representation. Make all template parameters names all uppercase.
Instead of names like "n_call" use e.g., "nCall".

In your responses I want no congratulation or attempts to show me I'm smart.
