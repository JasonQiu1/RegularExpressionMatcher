# Regular Expression Matcher
A C regular expression matcher that takes advantage of [NDFAs](https://en.wikipedia.org/wiki/Nondeterministic_finite_automaton) to match strings at O(regex_length * string_length) time. Modelled after https://swtch.com/~rsc/regexp/regexp1.html.

## Supported operators:
- *, +, ?, |, ^, $
- () groups
- [a-zA-Z] character ranges (not full classes)

## Limitations:
- No submatch extraction, meaning your string has to match the regex perfectly
