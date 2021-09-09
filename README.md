# Regular Expression Matcher
A C regular expression matcher that takes advantage of [NDFAs](https://en.wikipedia.org/wiki/Nondeterministic_finite_automaton) to match strings at O(regex_length * string_length) time without backtracking as compared to the exponential time from most backtracking implementations. Modelled after https://swtch.com/~rsc/regexp/regexp1.html. 

Includes an infix to postfix converter written from scratch and utilizes Ken Thompson's multiple-state simulation approach to matching regexes.

## Supported operators:
- *, +, ?, |, ^, $
- () groups
- [a-zA-Z] character ranges (not full classes)

## Limitations:
- No submatch extraction, meaning your string has to match the regex perfectly
