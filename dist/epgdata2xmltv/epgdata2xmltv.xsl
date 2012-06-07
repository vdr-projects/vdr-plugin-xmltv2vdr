<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:exsl="http://exslt.org/common"
                xmlns:date="http://exslt.org/dates-and-times"
                xmlns:str="http://exslt.org/strings">
<xsl:output method="xml" omit-xml-declaration="yes" encoding='utf-8'/> 

<xsl:template match="/">
<xsl:for-each select="//data[d2=$channelnum]">
<!-- <xsl:sort select="d4"/> --> 
<xsl:variable name="EVENTID">
<xsl:value-of select="d0"/>
</xsl:variable>

<xsl:variable name="INHALT">
<xsl:value-of select="translate(d22,'|','&#x0A;')"/>
</xsl:variable>

<xsl:variable name="THEMEN">
<xsl:value-of select="translate(d24,'|','&#x0A;')"/>
</xsl:variable>

<xsl:variable name="JAHR">
<xsl:value-of select="d33"/>
</xsl:variable>

<xsl:variable name="LAND">
<xsl:value-of select="translate(d32,'|','/')"/>
</xsl:variable>

<xsl:variable name="GENRE">
G <xsl:value-of select="d25"/>
</xsl:variable>

<xsl:variable name="EPISODE">
<xsl:if test="d26 &gt; 0"><xsl:value-of select="d26 - 1"/></xsl:if>
</xsl:variable>

<xsl:variable name="PICS">
<xsl:if test="string-length(d38)">
<xsl:element name="icon">
<xsl:attribute name="src">file:///var/lib/epgsources/epgdata2xmltv-img/<xsl:value-of select="d38"/></xsl:attribute>
</xsl:element>
<xsl:text>&#x0A;</xsl:text>
</xsl:if>
<xsl:if test="string-length(d39)">
<xsl:element name="icon">
<xsl:attribute name="src">file:///var/lib/epgsources/epgdata2xmltv-img/<xsl:value-of select="d39"/></xsl:attribute>
</xsl:element>
<xsl:text>&#x0A;</xsl:text>
</xsl:if>
<xsl:if test="string-length(d40)">
<xsl:element name="icon">
<xsl:attribute name="src">file:///var/lib/epgsources/epgdata2xmltv-img/<xsl:value-of select="d40"/></xsl:attribute>
</xsl:element>
<xsl:text>&#x0A;</xsl:text>
</xsl:if>
</xsl:variable>

<xsl:variable name="CREW">
<xsl:if test="string-length(d36)">
<xsl:call-template name="output-tokens">
<xsl:with-param name="list"><xsl:value-of select="d36"/></xsl:with-param>
<xsl:with-param name="tag">director</xsl:with-param>
<xsl:with-param name="delimiter">|</xsl:with-param>
</xsl:call-template>
</xsl:if>

<xsl:if test="string-length(d37)">
<xsl:call-template name="output-tokens">
<xsl:with-param name="list"><xsl:value-of select="d37"/></xsl:with-param>
<xsl:with-param name="tag">actor</xsl:with-param>
<xsl:with-param name="delimiter"> - </xsl:with-param>
</xsl:call-template>
</xsl:if>

<xsl:if test="string-length(d34)">
<xsl:call-template name="output-tokens">
<xsl:with-param name="list"><xsl:value-of select="d34"/></xsl:with-param>
<xsl:with-param name="tag">presenter</xsl:with-param>
<xsl:with-param name="delimiter">|</xsl:with-param>
</xsl:call-template>
</xsl:if>

<xsl:if test="string-length(d35)">
<xsl:call-template name="output-tokens">
<xsl:with-param name="list"><xsl:value-of select="d35"/></xsl:with-param>
<xsl:with-param name="tag">guest</xsl:with-param>
<xsl:with-param name="delimiter"> - </xsl:with-param>
</xsl:call-template>
</xsl:if>

</xsl:variable>

<xsl:variable name="VIDEO">

<xsl:if test="d29='1'">
<aspect>16:9</aspect><xsl:text>&#x0A;</xsl:text>
</xsl:if>

<xsl:if test="d11='1'">
<colour>no</colour><xsl:text>&#x0A;</xsl:text>
</xsl:if>

</xsl:variable>

<xsl:variable name="AUDIO">
<xsl:choose>
<xsl:when test="d28='1'">
    <xsl:text>dolby digital</xsl:text>
</xsl:when>
<!--
<xsl:when test="d12='1'">
    <xsl:text>bilingual</xsl:text>
</xsl:when>
-->
<xsl:when test="d27='1'">
    <xsl:text>stereo</xsl:text>
</xsl:when>
</xsl:choose>
</xsl:variable>

<xsl:variable name="STARRATING">
<xsl:if test="d30 &gt; 0">
<star-rating><value><xsl:value-of select="d30"/><xsl:text>/5</xsl:text></value></star-rating><xsl:text>&#x0A;</xsl:text>
</xsl:if>
</xsl:variable>

<xsl:variable name="TIPP">
<xsl:if test="d18=1">
<star-rating system="TagesTipp"><value>1/1</value></star-rating><xsl:text>&#x0A;</xsl:text>
</xsl:if>
<xsl:if test="d18=2">
<star-rating system="TopTipp"><value>1/1</value></star-rating><xsl:text>&#x0A;</xsl:text>
</xsl:if>
</xsl:variable>

<xsl:variable name="vps_iso8601">
<xsl:if test="string-length(d8)">
<xsl:call-template name="date2UTC">
<xsl:with-param name="date" select="concat(substring-before(d4,' '),'T',d8,':00')"/>
</xsl:call-template>
</xsl:if>
</xsl:variable>

<xsl:variable name="start_iso8601">
<xsl:call-template name="date2UTC">
<xsl:with-param name="date" select="concat(substring-before(d4,' '),'T',substring-after(d4,' '))"/>
</xsl:call-template>
</xsl:variable> 

<xsl:variable name="stop_iso8601">
<xsl:call-template name="date2UTC">
<xsl:with-param name="date" select="concat(substring-before(d5,' '),'T',substring-after(d5,' '))"/>
</xsl:call-template>
</xsl:variable>

<xsl:variable name="start_xmltv">
<xsl:value-of select="concat(translate($start_iso8601,'-:ZT',''),' +0000')"/>
</xsl:variable>

<xsl:variable name="vps_xmltv">
<xsl:if test="string-length($vps_iso8601)"> 
<xsl:value-of select="concat(translate($vps_iso8601,'-:ZT',''),' +0000')"/>
</xsl:if>
</xsl:variable>

<xsl:variable name="stop_xmltv">
<xsl:value-of select="concat(translate($stop_iso8601,'-:ZT',''),' +0000')"/>
</xsl:variable>

<xsl:element name="programme">
<xsl:attribute name="start">
<xsl:value-of select="$start_xmltv"/>
</xsl:attribute>
<xsl:attribute name="stop">
<xsl:value-of select="$stop_xmltv"/>
</xsl:attribute>
<xsl:if test="string-length($vps_xmltv)">
<xsl:attribute name="vps-start">
<xsl:value-of select="$vps_xmltv"/>
</xsl:attribute>
</xsl:if>
<xsl:attribute name="channel">
<xsl:value-of select="$channelid"/>
</xsl:attribute>
<xsl:text>&#x0A;</xsl:text>

<xsl:if test="string-length($EVENTID)">
<xsl:comment> pid = <xsl:value-of select="$EVENTID"/><xsl:text> </xsl:text></xsl:comment><xsl:text>&#x0A;</xsl:text>
</xsl:if>

<title lang="de"><xsl:value-of select="d19"/></title><xsl:text>&#x0A;</xsl:text>
<xsl:if test="string-length(d20)">
<sub-title lang="de"><xsl:value-of select="d20"/></sub-title><xsl:text>&#x0A;</xsl:text>
</xsl:if>
<xsl:if test="string-length($THEMEN)">
<desc lang="de"><xsl:value-of select="$THEMEN"/></desc><xsl:text>&#x0A;</xsl:text>
</xsl:if>
<xsl:if test="string-length($INHALT)">
<desc lang="de"><xsl:value-of select="$INHALT"/></desc><xsl:text>&#x0A;</xsl:text>
</xsl:if>
<xsl:if test="string-length($CREW)">
<credits><xsl:text>&#x0A;</xsl:text><xsl:copy-of select="$CREW"/></credits><xsl:text>&#x0A;</xsl:text>
</xsl:if>
<xsl:if test="string-length($JAHR)">
<date><xsl:value-of select="$JAHR"/></date><xsl:text>&#x0A;</xsl:text>
</xsl:if>
<xsl:if test="string-length($GENRE)">
<category lang="de"><xsl:value-of select="$GENRE"/></category><xsl:text>&#x0A;</xsl:text>
</xsl:if>
<xsl:if test="string-length($PICS)">
<xsl:copy-of select="$PICS"/>
</xsl:if>
<xsl:if test="string-length($LAND)">
<country><xsl:value-of select="$LAND"/></country><xsl:text>&#x0A;</xsl:text>
</xsl:if>
<xsl:if test="string-length($EPISODE)">
<episode-num system='xmltv_ns'>.<xsl:value-of select="$EPISODE"/>.</episode-num><xsl:text>&#x0A;</xsl:text>
</xsl:if>
<xsl:if test="string-length($VIDEO)">
<video><xsl:text>&#x0A;</xsl:text><xsl:copy-of select="$VIDEO"/></video><xsl:text>&#x0A;</xsl:text>
</xsl:if>
<xsl:if test="string-length($AUDIO)">
<audio><xsl:text>&#x0A;</xsl:text><stereo><xsl:value-of select="$AUDIO"/></stereo><xsl:text>&#x0A;</xsl:text></audio><xsl:text>&#x0A;</xsl:text>
</xsl:if>

<xsl:if test="string-length($STARRATING)">
<xsl:copy-of select="$STARRATING"/>
</xsl:if>
<xsl:if test="string-length($TIPP)">
<xsl:copy-of select="$TIPP"/>
</xsl:if>

</xsl:element>
<xsl:text>&#x0A;</xsl:text>
</xsl:for-each>
</xsl:template>

<xsl:template name="date2UTC">
  <xsl:param name="date"/>

  <xsl:variable name="dststart">
    <xsl:value-of select="concat(date:year($date),'-03-',32-date:day-in-week(concat(date:year($date),'-03-31')),'T02:00:00')"/>
  </xsl:variable>

  <xsl:variable name="dstend">
    <xsl:value-of select="concat(date:year($date),'-10-',32-date:day-in-week(concat(date:year($date),'-10-31')),'T03:00:00')"/>
  </xsl:variable>

  <xsl:variable name="tz">
  <xsl:choose>
  <xsl:when test="date:seconds(date:difference($dststart,$date)) &gt;= 0">
    <xsl:choose>
    <xsl:when test="date:seconds(date:difference($date,$dstend)) &gt;= 0">-PT2H</xsl:when>
    <xsl:otherwise>-PT1H</xsl:otherwise>
    </xsl:choose>
  </xsl:when>
  <xsl:otherwise>-PT1H</xsl:otherwise>
  </xsl:choose>
  </xsl:variable>

 <xsl:value-of select="date:add($date,$tz)"/>
</xsl:template>

<xsl:template name="output-tokens">
    <xsl:param name="list" />
    <xsl:param name="delimiter" />
    <xsl:param name="tag" />
    <xsl:param name="last"/>
    <xsl:variable name="newlist">
		<xsl:choose>
			<xsl:when test="contains($list, $delimiter)"><xsl:value-of select="normalize-space($list)" /></xsl:when>
			
			<xsl:otherwise><xsl:value-of select="concat(normalize-space($list), $delimiter)"/></xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
    <xsl:variable name="first" select="substring-before($newlist, $delimiter)" />
    <xsl:variable name="remaining" select="substring-after($newlist, $delimiter)" />
    <xsl:if test="$first != $last">
    <xsl:text disable-output-escaping="yes">&lt;</xsl:text><xsl:value-of select="$tag"/><xsl:text disable-output-escaping="yes">&gt;</xsl:text>
    <xsl:value-of select="$first" />
    <xsl:text disable-output-escaping="yes">&lt;/</xsl:text><xsl:value-of select="$tag"/><xsl:text disable-output-escaping="yes">&gt;</xsl:text>
    <xsl:text>&#x0A;</xsl:text>
    </xsl:if>
    <xsl:if test="$remaining">
        <xsl:call-template name="output-tokens">
            <xsl:with-param name="list" select="$remaining" />
            <xsl:with-param name="delimiter"><xsl:value-of select="$delimiter"/></xsl:with-param>
	    <xsl:with-param name="tag" select="$tag"/>
	    <xsl:with-param name="last" select="$first"/>
        </xsl:call-template>
    </xsl:if>
</xsl:template>



</xsl:stylesheet>
