const {exec} = require("child_process")
const gulp = require("gulp")

gulp.task("watch", () => {
	return gulp.watch("/wwwroot/**/*.mv", {usePolling: true}).on("change", path => {
		exec(`mvc -B /builtins ${path}`, (err, stdout, stderr) => {
			if (err) {
				console.error(stdout.toString())
				console.error(err)
			} else {
				console.log(`Compiled ${path}`)
			}
		})
	})
})