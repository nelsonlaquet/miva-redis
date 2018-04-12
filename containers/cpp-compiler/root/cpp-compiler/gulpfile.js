const {exec, spawnSync} = require("child_process")
const {basename, dirname, join} = require("path")
const {copySync} = require("fs-extra")
const gulp = require("gulp")

gulp.task("watch", () => {
	if (!process.env.PROJECT_DIRS) 
		throw new Error("You must specify the PROJECT_DIR environment variable, with a pipe deliniated series of paths!")
	
	// We get the PROJECT_DIRs env variable, which is in this format:
	// /path/to/project1->/artifact/path|/path/to/project2->/artifact/path
	// Where the path to the projects is where we will be listening (and running make) and the
	// artifact path is there the contents of the /path/to/project/bin directory will be copied to
	const projects = process.env.PROJECT_DIRS
		.split("|")
		.map(config => config.split("->"))
		.reduce((acc, [projectPath, artifactPath]) => {
			acc[projectPath] = {projectPath, artifactPath}
			return acc
		}, {})

	return gulp.watch(Object.keys(projects).map(dir => `${dir}/**/*.cpp`), {usePolling: true})
		.on("change", path => {
			// find which project this file was changed from...
			const project = projects[reduceOwn(projects, (acc, projectPath) => acc || (path.startsWith(projectPath) ? projectPath : null))]
			if (project == null) {
				console.error(`File '${path}' changed, but was not found in any project directories ${project.map(d => `'${d.projectPath}'`).join(", ")}!`);
				return
			}
			
			// execute make at the parent of the project directory...
			const {projectPath, artifactPath} = project
			exec(`make`, {cwd: projectPath}, (err, stdout, stderr) => {
				if (err) {
					console.error(stdout.toString())
					console.error(err)
				} else {
					// Copy the artifacts...
					copySync(join(projectPath, "bin"), artifactPath)
					console.log(`Ran make on '${path}'!`)
				}
			})
		})
})

function reduceOwn(obj, iterator, acc) {
	forEachOwn(obj, (key, value) => {
		acc = iterator(acc, key, value)
	})

	return acc
}

function forEachOwn(obj, iterator) {
	for (const key in obj) {
		if (!obj.hasOwnProperty(key))
			continue

		iterator(key, obj)
	}
}