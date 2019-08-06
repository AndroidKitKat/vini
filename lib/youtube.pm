#!/usr/bin/env perl

package youtube;

use warnings;
use strict;
use JSON;
use App::WatchLater::YouTube;
use URI::Encode qw(uri_decode);

use Number::Format;
my $nf = Number::Format->new(-thousands_sep   => '\'', -decimal_point   => ',');

my $gapikey = "AIzaSyCUYi5V6lrqKrgb9AfXeCl_NUj9xPk9nSs";
my $BOLD = "";

sub yt_search_video_id {
	my $text = shift;
	my $yt = App::WatchLater::YouTube->new(api_key => $gapikey);

	my $body = $yt->request("GET", "/search",
		part => "snippet",
		key => $gapikey,
		q => $text,
		maxResults => 1
	);

	my $id;
	if ($body =~ /\"videoId\"\:\s\"(.+)\"/) {
		$id = $1;
	}

	return $id;
}

sub yt_get_video_info {
	my $id = shift;
	my $yt = App::WatchLater::YouTube->new(api_key => $gapikey);

	my $stats = decode_json($yt->request("GET", "/videos",
		part => "statistics",
		key => $gapikey,
		id => $id
	));
	$stats = ${ ${ $stats->{items} }[0] }{statistics};

	my $snippet = decode_json($yt->request("GET", "/videos",
		part => "snippet",
		key => $gapikey,
		id => $id
	));
	$snippet = ${ ${ $snippet->{items} }[0] }{snippet};

	my $cdetails = decode_json($yt->request("GET", "/videos",
		part => "contentDetails",
		key => $gapikey,
		id => $id
	));

	my %info;

	$info{title} = $snippet->{title};
	$info{author} = $snippet->{channelTitle};
	$info{length} = yt_format_length(${ ${ ${ $cdetails->{items} }[0] }{contentDetails} }{duration});
	$info{vc} = $nf->format_number($stats->{viewCount});
	$info{likes} = $nf->format_number($stats->{likeCount});
	$info{dislikes} = $nf->format_number($stats->{dislikeCount});

	return %info;
}

sub yt_format_length {
	my $length = shift;
	my $minutes;
	my $seconds;
	my $fmt = "";

	if ($length =~ /PT(?:(\d*)H)?(?:(\d*)M)?(?:(\d*)S)?/) {
		if ($1 && $2 && $3) {
			$fmt = sprintf("%.2d:%.2d:%.2d", $nf->format_number($1), $2, $3);
		} elsif ($2 && $3) {
			$fmt = sprintf("%.2d:%.2d", $nf->format_number($2), $3);
		} elsif ($3) {
			$fmt = sprintf("00:%.2d", $nf->format_number($3));
		} else {
			$fmt = "unknown";
		}
	}

	return $fmt;
}

sub yt_output {
	my $id = shift;
	if (!($id)) {
		return "No results.";
	}

	my %info = yt_get_video_info($id);
	if (!(%info) || !($info{title})) {
		return "No results.";
	}

	my $title = $info{title};
	my $author = $info{author};
	my $length = $info{length};
	my $vc = $info{vc};
	my $likes = $info{likes};
	my $dislikes = $info{dislikes};

	my $out = "\xAB " . $BOLD . $title . $BOLD .
	    " [" . $BOLD . $length. $BOLD . "] " .
	    "(" . $BOLD . $vc . " views, " .
	    $likes . " likes, " . $dislikes . " dislikes" . $BOLD . ") " .
	    "uploaded by " .  $BOLD . $author . $BOLD . " \xBB";

	return $out;
}
