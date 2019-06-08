#!/usr/bin/perl

my ($chromsDir,$fa_list)=@ARGV;

open (IN, $fa_list) || die "can't open $fa_list\n";


while (defined(my $line=<IN>)){
 my @fas = split(' ',$line);
 my $refName="ref";
 my $cmdStr="cat ";
 foreach my $fa (@fas){
  my $bare_fa=substr($fa,0,-3);
  $refName.='_'.$bare_fa; 
  $cmdStr.=" ".$chromsDir.'/'.$fa
 }
 unless (-e "$refName"){
  print STDERR "mkdir -p $refName\n";
  system("mkdir -p $refName");
  print STDERR "$cmdStr > $refName/reference.fa\n";
  system("$cmdStr > $refName/reference.fa");
  print STDERR "cd $refName && bwa index -a bwtsw reference.fa\n";
  system("cd $refName && bwa index -a bwtsw reference.fa");
 }
}
