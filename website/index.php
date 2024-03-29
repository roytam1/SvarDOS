<!DOCTYPE html>
<html>
  <head>
<?php
  include 'mateuszbb.php';

  $chapters = array('' => 'Main page',
                    'repo' => 'Packages',
                    'help' => 'Help',
                    'forum' => 'Forum');

  /* pages available through ?p=xxx but not listed in main menu */
  $hidden_pages = array('files');

  $p = '';
  /* validate that ?p=xxx targets a valid subpage */
  if (! empty($_GET['p'])) {
    if (!empty($chapters[$_GET['p']])) $p = $_GET['p'];
    if (in_array($_GET['p'], $hidden_pages, true)) $p = $_GET['p'];
  }

  echo '    <title>';
  if (empty($p)) {
    echo 'SvarDOS';
  } else if (($p === 'forum') && (!empty($_GET['thread']))) {
    echo htmlspecialchars(mateuszbb_tytulwatku($_GET['thread']));
  } else {
    echo 'SvarDOS ' . $chapters[$p];
  }
  echo "</title>\n";
?>
    <meta name="keywords" content="svardos,svarog386,freedos">
    <meta name="author" content="Mateusz Viste">
    <meta name="robots" content="index, follow">
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="style.css">
  </head>
  <body>

  <p style="margin: 0 0 -1em auto; font-size: 0.9em; text-align: right;"><a href="/">Main page</a> I <a href="?p=repo">Packages</a> I <a href="?p=help">Help</a> I <a href="?p=forum">Forum</a>  <!-- I <a href="?p=nls">NLS</a>--></p>

<?php

  if (empty($p)) {
    include 'index-main.php';
  } else {
    include "index-{$p}.php";
  }

?>
  </body>
</html>
