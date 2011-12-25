#!/usr/bin/perl -w

use strict;

my $min_distance = 0xffffff;
my $max_distance = 0;

my $freq_old = 0;

while (my $line = <STDIN>) {
	my ($freq) = $line =~ /^(\d+) /;

	if ($freq_old != 0) {
		my $diff = $freq - $freq_old;
		if ($diff < $min_distance) {
			$min_distance = $diff;
		}
		if ($diff > $max_distance) {
			printf("New max distance at %u vs %u Hz: %u
				Hz\n", $freq_old, $freq, $diff);
			$max_distance = $diff;
		}
	}
	$freq_old = $freq;
}

printf("Min distance = %u Hz\n", $min_distance);
printf("Max distance = %u Hz\n", $max_distance);
