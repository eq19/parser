import { createWebpackConfig } from "./webpack_base.config.js";

export default createWebpackConfig({
  minimize: false,
  filename: "chevrotain.internal.temp.js",
});
