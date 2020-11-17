(function ($) {

/**
 * Provide the summary information for the block settings vertical tabs.
 */
Backdrop.behaviors.currentSearch = {
  attach: function (context) {
    // The backdropSetSummary method required for this behavior is not available
    // on the Blocks administration page, so we need to make sure this
    // behavior is processed only if backdropSetSummary is defined.
    if (typeof jQuery.fn.backdropSetSummary == 'undefined') {
      return;
    }

    $('fieldset#edit-current-search', context).backdropSetSummary(function (context) {
      var $radio = $('input[name="searcher"]:checked', context);
      return $radio.next('label').text();
    });
  }
};

})(jQuery);
