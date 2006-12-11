#!/bin/sh

mkdir -p jvlc-eclipse/src
mkdir jvlc-eclipse/bin

echo "<?xml version="1.0" encoding="UTF-8"?>" > jvlc-eclipse/.classpath
echo "<classpath>"  >> jvlc-eclipse/.classpath
echo "	<classpathentry kind="src" path="src"/>" >> jvlc-eclipse/.classpath
echo "	<classpathentry kind="con" path="org.eclipse.jdt.launching.JRE_CONTAINER"/>" >> jvlc-eclipse/.classpath
echo "	<classpathentry kind="output" path="bin"/>" >> jvlc-eclipse/.classpath
echo "</classpath>" >> jvlc-eclipse/.classpath


echo "<?xml version="1.0" encoding="UTF-8"?>" > jvlc-eclipse/.project
echo "<projectDescription>" >> jvlc-eclipse/.project
echo "	<name>jvlc-eclipse</name>" >> jvlc-eclipse/.project
echo "	<comment></comment>" >> jvlc-eclipse/.project
echo "	<projects>" >> jvlc-eclipse/.project
echo "	</projects>" >> jvlc-eclipse/.project
echo "	<buildSpec>" >> jvlc-eclipse/.project
echo "		<buildCommand>" >> jvlc-eclipse/.project
echo "			<name>org.eclipse.jdt.core.javabuilder</name>" >> jvlc-eclipse/.project
echo "			<arguments>" >> jvlc-eclipse/.project
echo "			</arguments>" >> jvlc-eclipse/.project
echo "		</buildCommand>" >> jvlc-eclipse/.project
echo "		<buildCommand>" >> jvlc-eclipse/.project
echo "			<name>org.eclipse.pde.ManifestBuilder</name>" >> jvlc-eclipse/.project
echo "			<arguments>" >> jvlc-eclipse/.project
echo "			</arguments>" >> jvlc-eclipse/.project
echo "		</buildCommand>" >> jvlc-eclipse/.project
echo "		<buildCommand>" >> jvlc-eclipse/.project
echo "			<name>org.eclipse.pde.SchemaBuilder</name>" >> jvlc-eclipse/.project
echo "			<arguments>" >> jvlc-eclipse/.project
echo "			</arguments>" >> jvlc-eclipse/.project
echo "		</buildCommand>" >> jvlc-eclipse/.project
echo "		<buildCommand>" >> jvlc-eclipse/.project
echo "			<name>com.atlassw.tools.eclipse.checkstyle.CheckstyleBuilder</name>" >> jvlc-eclipse/.project
echo "			<arguments>" >> jvlc-eclipse/.project
echo "			</arguments>" >> jvlc-eclipse/.project
echo "		</buildCommand>" >> jvlc-eclipse/.project
echo "	</buildSpec>" >> jvlc-eclipse/.project
echo "	<natures>" >> jvlc-eclipse/.project
echo "		<nature>org.eclipse.jdt.core.javanature</nature>" >> jvlc-eclipse/.project
echo "	</natures>" >> jvlc-eclipse/.project
echo "</projectDescription>" >> jvlc-eclipse/.project

cd jvlc-eclipse/src
ln -s ../../org


