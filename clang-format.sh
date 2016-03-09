STYLE=$(which clang-format)
if [ $? -ne 0 ]; then
	echo "clang-format not installed. Unable to check source file format policy." >&2
	exit 1
fi
RE=0
ALLFILES=$(find . -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.cc' -o -name '*.hh')
for FILE in $ALLFILES; do
  $STYLE $FILE | cmp -s $FILE -
  if [ $? -ne 0 ]; then
        echo "$FILE does not respect the coding style. Formatting. " >&2
        $STYLE -i $FILE 
        echo
        RE=1
  fi 
done

exit $RE
