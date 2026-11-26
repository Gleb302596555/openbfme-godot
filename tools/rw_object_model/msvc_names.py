"""The scope of an MSVC-decorated name: the namespaces / classes that qualify the symbol, innermost first.

Only the grammar the game's names use is accepted; anything else returns None (the caller then proves nothing)."""
from __future__ import annotations


class _Fail(Exception):
    pass


class _P:
    def __init__(self, s: str):
        self.s, self.i = s, 0
        self.names: list[str] = []

    def peek(self, n=1):
        return self.s[self.i:self.i + n]

    def take(self, n=1):
        if self.i + n > len(self.s):
            raise _Fail
        t = self.s[self.i:self.i + n]
        self.i += n
        return t

    def until_at(self):
        j = self.s.find("@", self.i)
        if j < 0:
            raise _Fail
        t = self.s[self.i:j]
        self.i = j + 1
        return t

    def remember(self, n):
        if len(self.names) < 10 and n not in self.names:
            self.names.append(n)

    # one name fragment: a simple name, a template name or a back-reference
    def fragment(self, scope=False):
        c = self.peek()
        if c.isdigit():
            self.take()
            k = int(c)
            if k >= len(self.names):
                raise _Fail
            return self.names[k]
        if self.peek(2) == "?$":
            self.take(2)
            saved = self.names
            self.names = []
            name = self.until_at()
            self.remember(name)
            self.targs()
            self.names = saved
            full = name + "<>"
            self.remember(full)
            return full
        if c == "?":
            if scope and self.peek(2) == "?A":  # anonymous namespace ?A0x1234@
                self.take(2)
                return "`anonymous-" + self.until_at() + "'"
            raise _Fail  # nested function scopes, operators inside a scope: not handled
        n = self.until_at()
        if not n:
            raise _Fail
        self.remember(n)
        return n

    def qualified(self):
        """Fragments up to the terminating '@' (the first one included)."""
        parts = [self.fragment()]
        while self.peek() != "@":
            parts.append(self.fragment(scope=True))
        self.take()
        return parts

    def number(self):
        if self.peek() == "?":
            self.take()
        c = self.take()
        if c.isdigit():
            return
        if not ("A" <= c <= "P"):
            raise _Fail
        while self.peek() != "@":
            if not ("A" <= self.take() <= "P"):
                raise _Fail
        self.take()

    def targs(self):
        while self.peek() != "@":
            if self.peek(2) == "$0":
                self.take(2)
                self.number()
            else:
                self.type()
        self.take()

    def cv(self):
        if self.take() not in "ABCD":
            raise _Fail

    def type(self):
        c = self.take()
        if c.isdigit():
            return
        if c in "CDEFGHIJKMNOXZ":
            return
        if c == "_":
            if self.take() not in "DEFGHIJKLMNOWX":
                raise _Fail
            return
        if c in "PQRS":
            if self.peek() == "6":  # pointer to a free function: calling convention, return type, arguments, throw spec
                self.take()
                if self.take() not in "AEGI":
                    raise _Fail
                if self.peek() == "?":
                    self.take()
                    self.cv()
                self.type()
                if self.peek() == "X":
                    self.take()
                else:
                    while self.peek() not in ("@", "Z", ""):
                        self.type()
                    self.take()
                if self.take() != "Z":
                    raise _Fail
                return
            self.cv()
            self.type()
            return
        if c in "AB":
            self.cv()
            self.type()
            return
        if c in "UVT":
            self.qualified()
            return
        if c == "W":
            if self.take() != "4":
                raise _Fail
            self.qualified()
            return
        if c == "$":
            d = self.take(2)
            if d in ("$C", "$Q", "$R"):
                self.cv()
                self.type()
                return
            if d == "$B":
                self.type()
                return
        raise _Fail


def scope(mangled: str):
    """['_STL'] for `?insert@?$map@...@_STL@@QAE...`; innermost first, the outermost last. None when the name is not a decorated
    C++ name of the shapes above."""
    if not mangled or not mangled.startswith("?"):
        return None
    p = _P(mangled)
    p.take()
    try:
        if p.peek(2) == "?$":
            p.fragment()
        elif p.peek() == "?":
            p.take()
            if p.peek() == "_":
                p.take(2)
            else:
                p.take()
        else:
            p.fragment()
        parts = []
        while p.peek() != "@":
            parts.append(p.fragment(scope=True))
        p.take()
    except (_Fail, IndexError):
        return None
    return parts
