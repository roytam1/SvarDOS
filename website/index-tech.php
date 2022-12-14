<?php
  $arts = array(
  'support'     => 'How do I get in touch with the SvarDOS community?',
  'pkgincl'     => 'Packages inclusion rules',
  'pkgformat'   => 'Package format',
  'licensing'   => 'SvarDOS licensing',
  'vboxfonts'   => 'Under VirtualBox the language-specific glyphs appear broken',
  'whatmeans'   => 'What does "Svarog" mean?'
  );

  natcasesort($arts);

  if (isset($_GET['art'])) {
    $art = $_GET['art'];
    if (!array_key_exists($art, $arts)) {
      echo "<p>invalid article!</p>";
      return;
    } else {
      echo '<h1>' . htmlentities($arts[$art]) . "</h1>\n";
      echo "<p class=\"copyr\">This is an article from the SvarDOS tech base</p>\n";
      echo '<p class="tech">';
      echo htmlentities(file_get_contents('tech/' . $art . '.txt'));
      echo "</p>\n";
      return;
    }
  }
?>

<h1>SvarDOS technical notes</h1>
<p class="copyr">no documentation is perfect, this one's no exception</p>

<table>

<?php
foreach ($arts as $file=>$title) {
  echo "<tr><td><a href=\"?p=tech&art={$file}\">" . htmlentities($title) . "</a></td></tr>\n";
}
?>

</table>
