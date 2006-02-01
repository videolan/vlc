<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0' xmlns="http://www.w3.org/TR/xhtml1/transitional" exclude-result-prefixes="#default">

<xsl:import href="/usr/share/sgml/docbook/stylesheet/xsl/nwalsh/xhtml/docbook.xsl"/>

<xsl:param name="html.stylesheet" select="'../screen.css'"/>
<xsl:param name="toc.section.depth" select="1"/>
<xsl:param name="generate.toc" select="'book toc chapter nop'"/>

<xsl:output method="xml" encoding="UTF-8" indent="yes" doctype-public="-//W3C//DTD XHTML 1.0 Transitional//EN" doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"/>

</xsl:stylesheet>

