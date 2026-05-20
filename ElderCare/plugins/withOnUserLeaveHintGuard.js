// plugins/withOnUserLeaveHintGuard.js
const { withMainActivity } = require('@expo/config-plugins');

module.exports = function withOnUserLeaveHintGuard(config) {
  return withMainActivity(config, (cfg) => {
    if (cfg.modResults.contents.includes('onUserLeaveHint')) {
      return cfg;
    }

    cfg.modResults.contents = cfg.modResults.contents.replace(
      /class MainActivity : ReactActivity\(\) \{/,
      `class MainActivity : ReactActivity() {
  override fun onUserLeaveHint() {
    try {
      super.onUserLeaveHint()
    } catch (e: NullPointerException) {
      // RN ReactDelegate not yet initialized on first launch — safe to swallow.
    }
  }
`
    );

    return cfg;
  });
};