# Rules to Follow

Code should be generalized and multiply functional not specific. Don't
hard code calculations as final values - show the calculation in compile
time constant expressions with variable names or comments or both.

In C and C++ I want to use camelCase instead of snake_case everywhere
and the "m_" prefix for instance members is not to be used. I want
2-space indentation rules, one space after each C++ keyword like if,
for, switch, while, etc. before the following expression in parentheses.
I want to use locally declared variables in for loops instead of
declaring these outside the for loop where possible. I want generic
index variables to be int. I want to use C string variables and arrays
instead of std::string or std::string_view where possible to avoid all
the verbosity and conversions to and from C string representation. Make
all template parameters names all uppercase. Instead of names like
"n_call" use e.g., "nCall". Even within camelCase acronymous are to be
in ALL CAPS so uQSDFoo1 is the right way to spell with the QSD acronym.
The exception is for when the acronym is the first of multi-word symbol
names like qsdSetMode which should make it lowercase when lowercase
would normally be used for that symbol.

Never use "_" in filenames unless you must for compatibility with APIs
or something like Python module names.

In code that uses four character ASCII values like 'abcd' (e.g., the
control plane for NexRx between app and twin or MCU), don't write the
value as hex constant in code with a comment saying what it is. Instead,
define a macro or compile-time-constant for each unique value and
reference that. Use in ControlHandler class something like `constexpr
uint32_t SVFO = 0x5356464F;` and then refer to ControlHandler's symbol
in switch/case statements and to construct the four byte CBOR uint32_t
values that should flow on the wire.

When appropriate, refactor source code that has accumulated a number of
modular components into separate module source files. Don't let our work
grow the size of source modules to such a size that handling them is
hard for you or for me.

In your responses I want no congratulation or attempts to show me I'm
smart.

Do _not_ do nuke-from-orbit rewrites of any non-trivial source module
without discussing with me first. Just do incremental edits to the files
unless we discuss making sweeping changes that require wholesale
replacement and I agree to this. If some tool you use fails that's NOT a
reason for you to completely rewrite a source file from your own context
memory, because that leads to a mess we have to back out anyway.

Never do a `git commit` unless I ask for it. Do not `git push` unless I
ask for a `git commit` and for it to be pushed.
