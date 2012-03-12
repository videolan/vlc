#!/bin/bash

# A script checking the po-files for translators. Final goal is updating AUTHORS.

 # To be copied and run in the git directory for having "ls po/*po" find the files.
 # It will generate a subdirectory temp_translators

echo Searching the headers of *.po

mkdir temp_translators
# Should check for existance, for the real use should be running a diff next time for reducing the lines to read

git describe > temp_translators/start.txt
date >> temp_translators/start.txt
# to have the date visible


for i in $( ls po/*po ); do
  echo item: $i
  echo $i >>temp_translators/start.txt

  if [ -h $i ]
  then
    echo just a link
    echo $i is just a link >>temp_translators/start.txt
  else


#  translationlangcode=$(echo $i | sed 's/\([a-z]*\).po*/\1/')
#  longlanguage=$(sed -n '1,30 s/[*]*[t,T]ranslati[.]*/&/p' < $i)
#  poeditlanguage=$(sed -n '1,30 s/[.]*X-Poedit-Language:[.]*/&/p' < $i)
#  echo $longlanguage $poeditlanguage
#  echo $translationlangcode >> ausgabe/start.txt
# I used this to have more output on running the script. Gives also the language name instead of the po-file's name only


  sed '/#: include/ q' < $i  >> temp_translators/start.txt
# The header should be done by now.
#TODO: A real safe way would take only the wanted meta parts (i.e. "Last-Translator:) and all commented lines #

  fi
done



# Removing every \n  The result will have to be read by humans, so there it's no use in keeping them
sed 's:\\n::g' < temp_translators/start.txt > temp_translators/start_without_backslash_n.txt

echo '"Plural-Forms:.*' > temp_translators/doubles.txt
# TODO: This can be more than one line ^^
echo '"&& n%100<=10 ? 3 : n%100>=11 && n%100<=99 ? 4 : 5;"' >> temp_translators/doubles.txt
echo '"%100<10 || n%100>=20) ? 1 : 2);"' >> temp_translators/doubles.txt
echo '"|| n%100>=20) ? 1 : 2);"' >> temp_translators/doubles.txt
echo '"%100==4 ? 3 : 0);"' >> temp_translators/doubles.txt
# The only two liners so far. Quick fix for the above TODO...

echo '"PO-Revision-Date: .*"' >> temp_translators/doubles.txt
echo '"X-Poedit-Bookmark.*' >> temp_translators/doubles.txt
echo '"Project-Id-Version:.*' >> temp_translators/doubles.txt
echo '"X-Generator: .*' >> temp_translators/doubles.txt
echo '"Language: .*' >> temp_translators/doubles.txt
echo ' *[Cc]opyright *([cC])[0-9 ,-]*t*h*e* *VideoLAN$* *t*e*a*m*[0-9 ,-]*\.*' >> temp_translators/doubles.txt
echo '[ ]*\$[ ]*[i,I][d,D][ ]*[:]*[ ]*\$' >> temp_translators/doubles.txt
echo '"X-Poedit-Country:.*' >> temp_translators/doubles.txt
echo '"X-Project-Style:.*'  >> temp_translators/doubles.txt
echo ' *<videolan@videolan.org> *' >> temp_translators/doubles.txt
# Whatever line occurs twice or more is most probably not a translators name. However, you can check it. The file will not be deleted
#TODO: If I knew sed better, I would put all the removed parts in a logfile. sort uniq would give a list to check for everything deleted
sort -r temp_translators/start_without_backslash_n.txt | uniq -d >> temp_translators/doubles.txt
# With the reverse Order the # will be removed at the end. Else the doubles beginning with # will not be matched

# Just check a "sort results.txt|less" for more to remove

# Changing the strings to sed commands removing the doubles
sed 's:.*:s^&^^g:' < temp_translators/doubles.txt > temp_translators/generated_com


# Removing all doubles
sed -f temp_translators/generated_com temp_translators/start_without_backslash_n.txt >temp_translators/results.txt


# Now, we are going to mark the already mentioned translators..
# Some names are written in CAPITALS. A modified marking script would be good.

sed -n  '/Localization/,/^$/ p' <AUTHORS | sed -n '3,$ s/\(.*\) --* .*/\1/p' >temp_translators/localization_part.txt
#sed -n '/Localization/,/^$/ p' <AUTHORS | sed -n '3,$ s/\(.*\) - .*/\1/p' >temp_translators/sect.txt
#The second line is what we want. I added -* to have "Éric Lassauge  -- French" included, too. But I really think this is a typo in AUTHORS

sed -n  '/Programming/,/^$/ p' <AUTHORS | sed  -n '3,$ p' >temp_translators/pro_part.txt
sed 's:.*:s^&^YYY-- & --YYY^g:' < temp_translators/pro_part.txt > temp_translators/replace_prog_names


# Changing the strings to sed commands removing the doubles
sed 's:.*:s^&^XXX-- & --XXX^g:' < temp_translators/localization_part.txt > temp_translators/replacenames



#mkdir -p temp_twice_used_adress
#echo "Checking all git logs"
#git shortlog -sne > temp_twice_used_adress/all_shortlog_sne.txt


#echo "Removing everything but email addresses"
git shortlog -sne |sed 's/[^<]*\(.*\)/\1/g' |sort > temp_translators/git_addresses_only.txt
#uniq -d|
sed 's:.*:s^&^ZZZ-- & --ZZZ^g:' < temp_translators/git_addresses_only.txt > temp_translators/replace_git_commiters



#sed -f temp_translators/replacenames <temp_translators/results.txt |uniq >temp_translators/review.txt

#sed -f temp_translators/replacenames <temp_translators/results.txt |sed -f temp_translators/replace_prog_names |uniq >temp_translators/review.txt
sed -f temp_translators/replacenames <temp_translators/results.txt |sed -f temp_translators/replace_prog_names |sed -f temp_translators/replace_git_commiters |uniq >temp_translators/review.txt

echo "Now temp_translators/review.txt should be reviewed. I don't think this can be done automatically, so I have done it already (not on your computer). Feedback is appreciated."
echo "XXX are named translators, YYY are named programmers, ZZZ commited with git"

# temp_translators/review.txt has to be reviewed manually. I don't think you would simply remove the names from AUTHORS found because then you would
# see an email adress and would not know, if it has to be added or how (with what name).
# The simpler to handle git commiter check script gives better names and dates, but of course not for all contributors. However, cross checking is recommended.

# The idea is to use a diff next time, but I did not work out yet what files would be best for that. Probably after finding the doubles would be a good time to remove everything visible in older versions.
