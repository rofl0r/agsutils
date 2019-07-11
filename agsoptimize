#!/usr/bin/env python2

# the AGS script compiler basically doesn't optimize at all,
# and the code it emits is super-naive and redundant.
# the register bx is most often used only as a temporary
# storage and discarded immediately after doing one basic op.
# therefore, doing these transformations should be safe.
# running these transformations on .s files reduces the
# amount of code by about 15-25%, but it also removes debugging
# information (sourceline directives).
# this speeds up game execution and makes the game smaller.
# currently, the optimizer takes input only from stdin and
# apply a single transformation at a time.
# a future optimization could be to cache code-chunks between
# jump labels and apply multiple transformation on the in-memory
# code.

import sys, re

class MultiLineMatcher():
	def __init__(self, regexes, matchfn, nomatchfn):
		self.matchfn = matchfn
		self.nomatchfn = nomatchfn
		self.regexes = regexes
		self.line_matches = 0
		self.saved_lines = None
		self.matches = None

	def feed(self, line):
		line = line.rstrip('\n')
		m = self.regexes[self.line_matches].match(line)
		if m:
			if self.matches is None: self.matches = []
			if self.saved_lines is None: self.saved_lines = []
			self.matches.append(m)
			self.saved_lines.append(line)
			self.line_matches += 1
			if self.line_matches == len(self.regexes):
				self.matchfn(self, self.matches, self.saved_lines)
				self.line_matches = 0
				self.saved_lines = None
		else:
			self.line_matches = 0
			self.matches = None
			if self.saved_lines:
				for ln in self.saved_lines:
					self.nomatchfn(self, ln)
				self.saved_lines = None
			self.nomatchfn(self, line)

removed = 0
lineno = 0

def push_pop_matchfn(matcher, matches, lines):
	ws, reg1 = matches[0].groups(0)
	ws2, reg2 = matches[1].groups(0)
	global removed
	if reg1 == reg2: removed += 1
	else: print "%smr %s, %s"%(ws, reg2, reg1)
	removed += 1

def output_fn(matcher, line):
	print line

def sourceline_matchfn(matcher, matches, lines):
	global removed
	removed += 1

def cmp_mr_matchfn(matcher, matches, lines):
	global removed
	ws, op = matches[0].groups(0)
	print "%s%s ax, bx"%(ws, op)
	removed += 1

def cmp2_matchfn(matcher, matches, lines):
	def reverse_cmp(op):
		if op == 'gt': return 'lte'
		elif op == 'gte': return 'lt'
		elif op == 'lt': return 'gte'
		elif op == 'lte': return 'gt'
	global removed
	ws, val = matches[1].groups(0)
	ws2, op = matches[2].groups(0)
	print "%sli bx, %s"%(ws, val)
	print "%s%s ax, bx"%(ws, op) # since we already switched registers, we don't need to switch the op too
	removed += 2

def load_negative_literal_matchfn(matcher, matches, lines):
	global removed
	ws, val = matches[1].groups(0)
	print "%smr bx, ax"%(ws)
	print "%sli ax, -%s"%(ws, val)
	removed += 4

def load_literal_matchfn(matcher, matches, lines):
	global removed
	ws, val = matches[1].groups(0)
	print "%smr bx, ax"%(ws)
	print "%sli ax, %s"%(ws, val)
	removed += 1

def axmar_matchfn(matcher, matches, lines):
	global removed
	ws, val = matches[0].groups(0)
	print "%sli bx, %s"%(ws, val)
	print lines[2]
	print lines[3]
	removed += 1

def mr_swap_matchfn(matcher, matches, lines):
	global removed
	print lines[0]
	removed += 1


def eprint(text): sys.stderr.write("%s\n"%text)

def usage():
	eprint("usage: %s -cmp -pushpop -sourceline -lnl -ll -cmp2 -axmar -mrswap"%sys.argv[0])
	eprint("")
	eprint("cmp: optimize cmp/mr")
	eprint("cmp2: optimize gt/gte/lt/lte (requires prev -ll pass)")
	eprint("pushpop: optimize push/pop")
	eprint("sourceline: remove sourceline statements")
	eprint("lnl: optimize negative literal loads")
	eprint("ll: optimize literal loads")
	eprint("only one option can be used at a time")
	eprint("input is taken from stdin")

def main():
	pushpop_matcher = MultiLineMatcher([
		re.compile('(\s+)push ([a-z]+)'),
		re.compile('(\s+)pop ([a-z]+)'),
	], push_pop_matchfn, output_fn)

	sourceline_matcher = MultiLineMatcher([
		re.compile('(\s+)sourceline ([0-9]+)'),
	], sourceline_matchfn, output_fn)

	cmp_mr_matcher = MultiLineMatcher([
		re.compile('(\s+)(cmpeq|cmpne|lor|land) bx, ax'),
		re.compile('(\s+)mr ax, bx'),
	], cmp_mr_matchfn, output_fn)

	load_negative_literal_matcher = MultiLineMatcher([
		re.compile('(\s+)push ax$'),
		re.compile('(\s+)li ax, ([0-9]+)$'),
		re.compile('(\s+)li bx, 0$'),
		re.compile('(\s+)sub bx, ax$'),
		re.compile('(\s+)mr ax, bx$'),
		re.compile('(\s+)pop bx$'),
	], load_negative_literal_matchfn, output_fn)

	load_literal_matcher = MultiLineMatcher([
		re.compile('(\s+)push ax$'),
		re.compile('(\s+)li ax, ([0-9]+)$'),
		re.compile('(\s+)pop bx$'),
	], load_literal_matchfn, output_fn)

	cmp2_matcher = MultiLineMatcher([
		re.compile('(\s+)mr bx, ax'),
		re.compile('(\s+)li ax, ([0-9]+)'),
		re.compile('(\s+)(gt|gte|lt|lte) bx, ax'),
		re.compile('(\s+)mr ax, bx'),
	], cmp2_matchfn, output_fn)

	axmar_matcher = MultiLineMatcher([
		re.compile('(\s+)li ax, ([0-9]+)$'),
		re.compile('(\s+)mr bx, ax$'),
		re.compile('(\s+)li mar, (.+)$'),
		re.compile('(\s+)mr ax, mar$'),
	], axmar_matchfn, output_fn)

	mr_swap_matcher = MultiLineMatcher([
		re.compile('(\s+)mr ax, bx'),
		re.compile('(\s+)mr bx, ax'),
	], mr_swap_matchfn, output_fn)

	global lineno
	if len(sys.argv) != 2: return usage()
	if sys.argv[1] == "-cmp": matcher = cmp_mr_matcher
	elif sys.argv[1] == "-cmp2": matcher = cmp2_matcher
	elif sys.argv[1] == "-pushpop": matcher = pushpop_matcher
	elif sys.argv[1] == "-sourceline": matcher = sourceline_matcher
	elif sys.argv[1] == "-lnl": matcher = load_negative_literal_matcher
	elif sys.argv[1] == "-ll": matcher = load_literal_matcher
	elif sys.argv[1] == "-axmar": matcher = axmar_matcher
	elif sys.argv[1] == "-mrswap": matcher = mr_swap_matcher
	else: return usage()
	while True:
		lineno = lineno + 1
		s = sys.stdin.readline()
		if s == '': break
		matcher.feed(s)

	sys.stderr.write( "removed %d lines\n"%removed)

if __name__ == "__main__": main()
