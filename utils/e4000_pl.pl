#!/usr/bin/perl -w

use strict;

sub pll_fvco($$$$) {
	my ($fosc, $z, $x, $y) = @_;

	return $fosc * ($z + ($x/$y));
}

sub pll_flo($$) {
	my ($fvco, $r_num) = @_;

	return $fvco  / $r_num;
}

sub is_flo_valid($) {
	my $flo = shift;

	if ($flo < 64000000 || $flo > 1700000000) {
		return 0;
	}

	return 1;
}

sub min_vco_mult($) {
	my ($fosc) = @_;
	return (2600000000 / $fosc);
}

sub max_vco_mult($) {
	my ($fosc) = @_;
	return (3900000000 / $fosc);
}

sub test_setting($$$$$) {
	my ($fosc, $z, $x, $y, $r) = @_;
	my $flo;
	my $fvco = pll_fvco($fosc, $z, $x, $y);

	if ($fvco < 2600000000 || $fvco > 3900000000) {
		return;
	}

	$flo = pll_flo($fvco, $r);
	if (is_flo_valid($flo)) {
		printf("%010u Hz (Z=%u, X=%u, R=%u)\n", $flo, $z, $x, $r);
	}

	$r = $r * 2;
	$flo = pll_flo($fvco, $r);
	if (is_flo_valid($flo) && $flo < 300000000) { 
		printf("%010u Hz (Z=%u, X=%u, R=%u, TPM)\n", $flo, $z, $x, $r);
	}
}


sub hr() {
	printf("======================================================================\n");
}

my $fosc = 26000000;
my $y = 65535;
my @r_int_vals = (2,4,6,8,12,16,20,24);


my $min_vco_mult = min_vco_mult($fosc);
my $max_vco_mult = max_vco_mult($fosc);

printf("Fosc = %u, min_vco_mult=%u, max_vco_mult=%u\n", $fosc,
	$min_vco_mult, $max_vco_mult);

for (my $z = $min_vco_mult; $z <= $max_vco_mult; $z++) {
	for (my $x = 0; $x <= 0xffff; $x+= 1) {
		foreach  my $r (@r_int_vals) {
			test_setting($fosc, $z, $x, $y, $r);
		}
	}
}


